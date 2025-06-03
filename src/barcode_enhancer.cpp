#include "barcode_enhancer.h"
#include <cmath>
#include <algorithm>
#include <memory>
#include <numeric>
#include <cstring>
#include <iostream>

namespace ZXing {

std::vector<uint8_t> Code128Enhancer::convertToGrayscale(const ImageView& input) {
    int width = input.width();
    int height = input.height();
    std::vector<uint8_t> result(width * height);

    if (input.format() == ImageFormat::Lum) {
        // Already grayscale, just copy
        std::memcpy(result.data(), input.data(), width * height);
    } else {
        // Convert RGB/RGBA to grayscale using standard weights
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const uint8_t* pixel = input.data(x, y);
                result[y * width + x] = static_cast<uint8_t>(
                    pixel[0] * 0.299f + 
                    pixel[1] * 0.587f + 
                    pixel[2] * 0.114f
                );
            }
        }
    }
    return result;
}

void Code128Enhancer::gaussianBlur(std::vector<uint8_t>& image, int width, int height, float sigma) {
    if (width <= 0 || height <= 0 || image.size() != static_cast<size_t>(width * height)) {
        return;
    }

    // Create Gaussian kernel
    const int kernelSize = static_cast<int>(std::ceil(sigma * 3) * 2 + 1);
    const int kernelRadius = kernelSize / 2;
    std::vector<float> kernel(kernelSize);
    float sum = 0.0f;

    for (int i = 0; i < kernelSize; i++) {
        float x = static_cast<float>(i - kernelRadius);
        kernel[i] = std::exp(-(x * x) / (2 * sigma * sigma));
        sum += kernel[i];
    }

    // Normalize kernel
    for (int i = 0; i < kernelSize; i++) {
        kernel[i] /= sum;
    }

    // Apply horizontal blur
    std::vector<uint8_t> temp(width * height);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float sum = 0.0f;
            float weightSum = 0.0f;

            for (int i = -kernelRadius; i <= kernelRadius; i++) {
                int nx = x + i;
                if (nx >= 0 && nx < width) {
                    float weight = kernel[i + kernelRadius];
                    sum += image[y * width + nx] * weight;
                    weightSum += weight;
                }
            }

            temp[y * width + x] = static_cast<uint8_t>(std::clamp(sum / weightSum, 0.0f, 255.0f));
        }
    }

    // Apply vertical blur
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float sum = 0.0f;
            float weightSum = 0.0f;

            for (int i = -kernelRadius; i <= kernelRadius; i++) {
                int ny = y + i;
                if (ny >= 0 && ny < height) {
                    float weight = kernel[i + kernelRadius];
                    sum += temp[ny * width + x] * weight;
                    weightSum += weight;
                }
            }

            image[y * width + x] = static_cast<uint8_t>(std::clamp(sum / weightSum, 0.0f, 255.0f));
        }
    }
}

void Code128Enhancer::adaptiveThreshold(std::vector<uint8_t>& image, int width, int height, int blockSize, float C) {
    if (image.empty() || width <= 0 || height <= 0) return;

    // Calculate mean and standard deviation
    double sum = 0.0, sumSq = 0.0;
    for (uint8_t pixel : image) {
        sum += pixel;
        sumSq += pixel * pixel;
    }
    double mean = sum / image.size();
    double variance = (sumSq / image.size()) - (mean * mean);
    double stdDev = std::sqrt(variance);

    // Adjust parameters based on image statistics
    bool isHighValueImage = mean > 120;
    float adaptiveC = isHighValueImage ? C * 0.7f : C;  // Lower threshold offset for bright images
    int adaptiveBlockSize = isHighValueImage ? blockSize * 3/2 : blockSize;  // Larger window for bright images

    // Create a copy of the original image
    std::vector<uint8_t> result = image;

    // Ensure block size is odd
    adaptiveBlockSize = (adaptiveBlockSize % 2 == 0) ? adaptiveBlockSize + 1 : adaptiveBlockSize;
    int offset = adaptiveBlockSize / 2;

    // For each pixel
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // Calculate local mean
            double localSum = 0.0;
            int count = 0;

            // Define block boundaries with edge handling
            int startY = std::max(0, y - offset);
            int endY = std::min(height - 1, y + offset);
            int startX = std::max(0, x - offset);
            int endX = std::min(width - 1, x + offset);

            // Calculate weighted sum (pixels closer to center have more influence)
            for (int by = startY; by <= endY; by++) {
                for (int bx = startX; bx <= endX; bx++) {
                    double weight = 1.0 - (std::abs(by - y) + std::abs(bx - x)) / (2.0 * offset);
                    localSum += image[by * width + bx] * weight;
                    count++;
                }
            }

            double threshold = (localSum / count) - adaptiveC;
            
            // For bright images, use a more sophisticated thresholding
            if (isHighValueImage) {
                // Calculate local contrast
                double localStdDev = 0.0;
                for (int by = startY; by <= endY; by++) {
                    for (int bx = startX; bx <= endX; bx++) {
                        double diff = image[by * width + bx] - (localSum / count);
                        localStdDev += diff * diff;
                    }
                }
                localStdDev = std::sqrt(localStdDev / count);

                // Adjust threshold based on local contrast
                if (localStdDev < stdDev * 0.5) {
                    // Low contrast region - be more conservative
                    threshold = (localSum / count) - adaptiveC * 0.5;
                } else {
                    // High contrast region - be more aggressive
                    threshold = (localSum / count) - adaptiveC * 1.2;
                }
            }

            result[y * width + x] = (image[y * width + x] < threshold) ? 0 : 255;
        }
    }

    // Copy result back to input image
    image = std::move(result);
}

void Code128Enhancer::applyDirectionalFilter(std::vector<uint8_t>& image, int width, int height, int kernelSize) {
    if (image.empty() || width <= 0 || height <= 0) return;
    
    // Create a copy of the original image
    std::vector<uint8_t> result = image;
    
    // Ensure kernel size is odd
    kernelSize = (kernelSize % 2 == 0) ? kernelSize + 1 : kernelSize;
    int halfKernel = kernelSize / 2;
    
    // Create a specialized kernel for Code 128 barcode enhancement
    // Code 128 has specific bar width patterns (1:2:3:4 module widths)
    std::vector<float> horizontalKernel(kernelSize);
    
    // Generate a directional kernel that emphasizes Code 128 bar patterns
    for (int i = 0; i < kernelSize; i++) {
        float x = static_cast<float>(i - halfKernel) / halfKernel;
        // Sharper transition kernel for better edge detection in Code 128
        // The exponent is reduced from 2 to 1.5 to create sharper transitions
        horizontalKernel[i] = -x * std::exp(-x * x * 1.5);
    }
    
    // Apply the directional filter with Code 128 specific enhancements
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float sum = 0.0f;
            float kernelSum = 0.0f;
            
            // Apply horizontal kernel
            for (int i = -halfKernel; i <= halfKernel; i++) {
                int nx = x + i;
                if (nx >= 0 && nx < width) {
                    float kernelValue = horizontalKernel[i + halfKernel];
                    sum += image[y * width + nx] * kernelValue;
                    kernelSum += std::abs(kernelValue);
                }
            }
            
            // Normalize and adjust the result with stronger contrast for Code 128
            // Increase contrast factor from 1.5 to 2.0 for better bar/space differentiation
            int filteredValue = static_cast<int>(128 + (sum / kernelSum) * 2.0f);
            result[y * width + x] = static_cast<uint8_t>(std::clamp(filteredValue, 0, 255));
        }
    }
    
    // Apply a second pass to enhance bar edges specifically for Code 128
    // This helps with detecting the precise edges of bars which is critical for Code 128
    for (int y = 0; y < height; y++) {
        for (int x = 1; x < width - 1; x++) {
            // Detect edges (transitions between bars and spaces)
            int left = result[y * width + (x-1)];
            int center = result[y * width + x];
            int right = result[y * width + (x+1)];
            
            // If this is an edge (significant difference between neighbors)
            if ((std::abs(left - center) > 30) || (std::abs(right - center) > 30)) {
                // Enhance the edge by making it more distinct
                if (center < 128) {
                    // Dark bar - make it darker
                    result[y * width + x] = 0;
                } else {
                    // Light space - make it lighter
                    result[y * width + x] = 255;
                }
            }
        }
    }
    
    // Copy result back to input image
    image = std::move(result);
}

ImageView Code128Enhancer::toImageView(const std::vector<uint8_t>& image, int width, int height) {
    // Create a copy of the image data that will be owned by the ImageView
    uint8_t* data = new uint8_t[width * height];
    std::memcpy(data, image.data(), width * height);
    
    // Return ImageView with the copied data
    return ImageView(data, width, height, ImageFormat::Lum);
}

ImageView Code128Enhancer::rotateImage(const ImageView& input, int degrees) {
    int width = input.width();
    int height = input.height();
    
    // Normalize degrees to 0-360
    degrees = ((degrees % 360) + 360) % 360;
    
    // Only handle 90-degree rotations for now
    if (degrees % 90 != 0) {
        return input;
    }
    
    // Create output buffer
    std::unique_ptr<uint8_t[]> outputBuffer;
    int outWidth, outHeight;
    
    if (degrees == 90 || degrees == 270) {
        outWidth = height;
        outHeight = width;
        outputBuffer = std::make_unique<uint8_t[]>(width * height);
        
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                if (degrees == 90) {
                    outputBuffer[x * height + (height - 1 - y)] = input.data()[y * width + x];
                } else {  // 270 degrees
                    outputBuffer[(width - 1 - x) * height + y] = input.data()[y * width + x];
                }
            }
        }
    } else if (degrees == 180) {
        outWidth = width;
        outHeight = height;
        outputBuffer = std::make_unique<uint8_t[]>(width * height);
        
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                outputBuffer[(height - 1 - y) * width + (width - 1 - x)] = input.data()[y * width + x];
            }
        }
    } else {  // 0 or 360 degrees
        return input;
    }
    
    return ImageView(outputBuffer.release(), outWidth, outHeight, ImageFormat::Lum);
}

ImageView Code128Enhancer::cropImage(const ImageView& input, int x, int y, int width, int height) {
    // Validate input parameters
    if (x < 0 || y < 0 || width <= 0 || height <= 0 ||
        x + width > input.width() || y + height > input.height()) {
        return input;
    }
    
    // Create output buffer
    std::unique_ptr<uint8_t[]> outputBuffer = std::make_unique<uint8_t[]>(width * height);
    
    // Copy region
    for (int ry = 0; ry < height; ry++) {
        std::memcpy(
            outputBuffer.get() + ry * width,
            input.data() + (y + ry) * input.width() + x,
            width
        );
    }
    
    return ImageView(outputBuffer.release(), width, height, ImageFormat::Lum);
}

ImageView Code128Enhancer::enhanceBarcode(const ImageView& input, bool shouldInvert, bool isTest) {
    // Validate input
    if (!input.data() || input.width() <= 0 || input.height() <= 0) {
        std::cout << "Invalid input image" << std::endl;
        return input;
    }

    // Try decoding original image first with Code 128 specific options
    ReaderOptions opts;
    opts.setTryHarder(true);
    opts.setTryRotate(true);
    opts.setIsPure(false);
    opts.setBinarizer(Binarizer::LocalAverage);
    opts.setFormats(BarcodeFormat::Code128);
    opts.setMinLineCount(2);

    // Try decoding the oriented image first
    auto result = ReadBarcode(input, opts);
    if (result.isValid()) {
        std::cout << "Oriented image decoded successfully" << std::endl;
        return input;
    }

    // Convert to grayscale and keep data alive
    std::vector<uint8_t> image = convertToGrayscale(input);
    int width = input.width();
    int height = input.height();

    // Parameters for Code 128 enhancement - optimized based on testing
    const int minModuleWidth = 1;  // Minimum expected width of a module in pixels
    const int maxModuleWidth = 15;  // Increased for very low-resolution images
    const int quietZoneWidth = 4;  // Further reduced for barcodes with minimal margins
    const double minContrast = 0.1;  // Very permissive for poor contrast images
    const int verticalRedundancy = static_cast<int>(height * 0.2);  // Increased to 20% for maximum noise reduction

    // std::cout << "Starting Code 128 enhancement with parameters:" << std::endl;
    // std::cout << "- Min module width: " << minModuleWidth << std::endl;
    // std::cout << "- Max module width: " << maxModuleWidth << std::endl;
    // std::cout << "- Quiet zone width: " << quietZoneWidth << std::endl;
    // std::cout << "- Min contrast: " << minContrast << std::endl;
    // std::cout << "- Vertical redundancy: " << verticalRedundancy << std::endl;

    // Apply vertical averaging to reduce noise
    std::vector<uint8_t> verticalAveraged(width * height);
    for (int y = 0; y < height; y++) {
        int startY = std::max(0, y - verticalRedundancy/2);
        int endY = std::min(height - 1, y + verticalRedundancy/2);
        for (int x = 0; x < width; x++) {
            int sum = 0;
            for (int avgY = startY; avgY <= endY; avgY++) {
                sum += image[avgY * width + x];
            }
            verticalAveraged[y * width + x] = static_cast<uint8_t>(sum / (endY - startY + 1));
        }
    }
    
    // Apply directional filtering to enhance horizontal patterns (Code 128 specific)
    applyDirectionalFilter(verticalAveraged, width, height, maxModuleWidth);

    // Apply adaptive thresholding with Code 128 specific parameters
    int blockSize = std::max(minModuleWidth * 4, 11);  // Block size based on module width
    float C = 5.0f;  // Threshold adjustment
    adaptiveThreshold(verticalAveraged, width, height, blockSize, C);
    
    // Apply bar width correction specifically for Code 128
    correctBarWidths(verticalAveraged, width, height);

    // Try decoding after thresholding
    std::unique_ptr<uint8_t[]> thresholdBuffer(new uint8_t[width * height]);
    std::memcpy(thresholdBuffer.get(), verticalAveraged.data(), width * height);
    ImageView thresholdView(thresholdBuffer.get(), width, height, ImageFormat::Lum);

    // no need to enhance image if original is decodable
    // If we are in test mode, we still want to apply enhancement to assess if it helps
    // if(isTest == false){
    //     result = ReadBarcode(thresholdView, opts);
    //     if (result.isValid()) {
    //         std::cout << "Thresholded image is decodable, returning it" << std::endl;
    //         return thresholdView;
    //     }
    // }

    // If we get here, try with inversion if requested (only when necessary)
    if (shouldInvert) {
        // In-place inversion to avoid extra memory allocation
        for (int i = 0; i < width * height; i++) {
            verticalAveraged[i] = 255 - verticalAveraged[i];
        }

        // Reuse the same buffer for the inverted view
        std::unique_ptr<uint8_t[]> invertedBuffer(new uint8_t[width * height]);
        std::memcpy(invertedBuffer.get(), verticalAveraged.data(), width * height);
        ImageView invertedView(invertedBuffer.get(), width, height, ImageFormat::Lum);

        auto result = ReadBarcode(invertedView, opts);
        if (result.isValid()) {
            std::cout << "Inverted image is decodable, returning it" << std::endl;
            return invertedView;
        }
    }

    // If nothing worked, return the original image
    std::cout << "No enhancement was successful, returning original image" << std::endl;
    return input;
}

ImageView Code128Enhancer::scaleImage(const ImageView& input, float scaleFactor) {
    if (scaleFactor <= 0 || !input.data()) {
        return input;
    }
    
    int originalWidth = input.width();
    int originalHeight = input.height();
    int newWidth = static_cast<int>(originalWidth * scaleFactor);
    int newHeight = static_cast<int>(originalHeight * scaleFactor);
    
    if (newWidth <= 0 || newHeight <= 0) {
        return input;
    }
    
    // Create output buffer
    std::unique_ptr<uint8_t[]> outputBuffer = std::make_unique<uint8_t[]>(newWidth * newHeight);
    
    // Use bilinear interpolation for scaling
    for (int y = 0; y < newHeight; y++) {
        float originalY = y / scaleFactor;
        int y1 = static_cast<int>(originalY);
        int y2 = std::min(y1 + 1, originalHeight - 1);
        float yFraction = originalY - y1;
        
        for (int x = 0; x < newWidth; x++) {
            float originalX = x / scaleFactor;
            int x1 = static_cast<int>(originalX);
            int x2 = std::min(x1 + 1, originalWidth - 1);
            float xFraction = originalX - x1;
            
            // Bilinear interpolation
            float p1 = input.data()[y1 * originalWidth + x1];
            float p2 = input.data()[y1 * originalWidth + x2];
            float p3 = input.data()[y2 * originalWidth + x1];
            float p4 = input.data()[y2 * originalWidth + x2];
            
            float top = p1 * (1 - xFraction) + p2 * xFraction;
            float bottom = p3 * (1 - xFraction) + p4 * xFraction;
            float pixel = top * (1 - yFraction) + bottom * yFraction;
            
            outputBuffer[y * newWidth + x] = static_cast<uint8_t>(std::clamp(pixel, 0.0f, 255.0f));
        }
    }
    
    return ImageView(outputBuffer.release(), newWidth, newHeight, ImageFormat::Lum);
}

void Code128Enhancer::correctBarWidths(std::vector<uint8_t>& image, int width, int height) {
    if (width <= 0 || height <= 0) {
        return;
    }
    
    // More aggressive bar width correction parameters for Code 128
    // Code 128 has specific module width ratios (1:2:3:4)
    // This array represents the expected relative widths of modules in Code 128
    const int moduleWidths[] = {1, 2, 3, 4};
    
    // Create a copy of the original image
    std::vector<uint8_t> result = image;
    
    // Sample multiple scan lines across the height of the image
    // This improves robustness against noise and distortion
    const int numScanLines = std::min(10, height / 2);
    const int scanLineStep = height / (numScanLines + 1);
    
    for (int scanLine = 0; scanLine < numScanLines; scanLine++) {
        int y = (scanLine + 1) * scanLineStep;
        if (y >= height) continue;
        
        // Find transitions between bars and spaces
        std::vector<int> transitions;
        std::vector<int> barWidths;
        
        // Start with a threshold-based approach to find transitions
        uint8_t lastPixel = image[y * width];
        bool isBar = (lastPixel < 128);
        int currentWidth = 1;
        
        // Scan the line to find transitions and measure bar/space widths
        for (int x = 1; x < width; x++) {
            uint8_t pixel = image[y * width + x];
            bool currentIsBar = (pixel < 128);
            
            if (currentIsBar != isBar) {
                // Transition detected
                transitions.push_back(x);
                barWidths.push_back(currentWidth);
                isBar = currentIsBar;
                currentWidth = 1;
            } else {
                currentWidth++;
            }
        }
        
        // Add the last segment if needed
        if (currentWidth > 0) {
            barWidths.push_back(currentWidth);
        }
        
        // Need at least 25 bars for a valid Code 128 barcode
        // (start/stop patterns + data + check digit)
        // Reduced from 30 to be more permissive with partial barcodes
        if (barWidths.size() < 25) continue;
        
        // Find the minimum bar width (X dimension)
        int minBarWidth = *std::min_element(barWidths.begin(), barWidths.end());
        if (minBarWidth <= 0) continue;
        
        // Calculate expected module widths based on the X dimension
        std::vector<int> expectedWidths(4);
        for (int i = 0; i < 4; i++) {
            expectedWidths[i] = moduleWidths[i] * minBarWidth;
        }
        
        // Correct bar widths to match expected module widths
        for (size_t i = 0; i < barWidths.size(); i++) {
            int measuredWidth = barWidths[i];
            
            // Find the closest expected module width
            int bestMatch = 0;
            int minDiff = std::abs(measuredWidth - expectedWidths[0]);
            
            for (int j = 1; j < 4; j++) {
                int diff = std::abs(measuredWidth - expectedWidths[j]);
                if (diff < minDiff) {
                    minDiff = diff;
                    bestMatch = j;
                }
            }
            
            // If the difference is significant, adjust the width
            // Using a more permissive threshold (minBarWidth / 2 instead of minBarWidth / 3)
            // to accommodate more distorted barcodes
            if (minDiff > minBarWidth / 2) {
                int startX = (i > 0) ? transitions[i-1] : 0;
                int endX = (i < transitions.size()) ? transitions[i] : width;
                int correctedWidth = expectedWidths[bestMatch];
                
                // Adjust the image to match the corrected width
                // This is a simplified approach - in practice, you might want to
                // adjust the positions of transitions rather than stretching/shrinking
                bool isCurrentBar = ((i % 2) == 0) ? true : false;
                uint8_t pixelValue = isCurrentBar ? 0 : 255;
                
                // Apply the correction to a larger vertical strip around the scan line
                // Increased from 5 to height/5 for more effective correction
                int stripHeight = std::min(height / 5, height / 2);
                int startY = std::max(0, y - stripHeight/2);
                int endY = std::min(height, y + stripHeight/2 + 1);
                
                for (int cy = startY; cy < endY; cy++) {
                    for (int cx = startX; cx < endX; cx++) {
                        result[cy * width + cx] = pixelValue;
                    }
                }
            }
        }
    }
    
    // Copy result back to input image
    image = std::move(result);
}

ImageView Code128Enhancer::enhanceBarcodeMultiScale(const ImageView& input, bool shouldInvert) {
    if (!input.data() || input.width() <= 0 || input.height() <= 0) {
        std::cout << "Invalid input image for multi-scale analysis" << std::endl;
        return input;
    }
    
    // Configure reader options specifically for Code 128
    ReaderOptions opts;
    opts.setTryHarder(true);
    opts.setTryRotate(false); // We'll handle rotation separately if needed
    opts.setIsPure(false);
    opts.setBinarizer(Binarizer::LocalAverage);
    opts.setFormats(BarcodeFormat::Code128);
    opts.setMinLineCount(2);
    
    // Try original scale first
    auto enhancedOriginal = enhanceBarcode(input, shouldInvert, true);
    auto result = ReadBarcode(enhancedOriginal, opts);
    
    if (result.isValid()) {
        std::cout << "Original scale enhanced image is decodable" << std::endl;
        return enhancedOriginal;
    }
    
    // Define scales to try - these are carefully chosen for Code 128 barcodes
    // which can vary in module width but maintain specific ratios
    const float scales[] = {0.75f, 1.25f, 1.5f, 2.0f};
    
    for (float scale : scales) {
        std::cout << "Trying scale factor: " << scale << std::endl;
        
        // Scale the image
        auto scaledImage = scaleImage(input, scale);
        
        // Enhance the scaled image
        auto enhancedScaled = enhanceBarcode(scaledImage, shouldInvert, true);
        
        // Try to decode
        result = ReadBarcode(enhancedScaled, opts);
        
        if (result.isValid()) {
            std::cout << "Successfully decoded at scale factor: " << scale << std::endl;
            return enhancedScaled;
        }
    }
    
    // If no scale worked, return the original enhanced version
    std::cout << "No scale factor succeeded, returning original enhanced image" << std::endl;
    return enhancedOriginal;
}

} // namespace ZXing 