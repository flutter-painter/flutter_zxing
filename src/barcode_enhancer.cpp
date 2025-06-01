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
    
    // Create a horizontal Sobel-like kernel for Code 128 barcode enhancement
    // This kernel will enhance vertical edges (horizontal transitions) which are
    // characteristic of Code 128 barcodes
    std::vector<float> horizontalKernel(kernelSize);
    
    // Generate a directional kernel that emphasizes horizontal patterns
    for (int i = 0; i < kernelSize; i++) {
        // Create a kernel that enhances horizontal patterns
        // Center of the kernel has highest weight, edges have negative weights
        float x = static_cast<float>(i - halfKernel) / halfKernel;
        horizontalKernel[i] = -x * std::exp(-x * x * 2);
    }
    
    // Apply the directional filter
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
            
            // Normalize and adjust the result
            // Scale to 0-255 range and enhance contrast
            int filteredValue = static_cast<int>(128 + (sum / kernelSum) * 1.5f);
            result[y * width + x] = static_cast<uint8_t>(std::clamp(filteredValue, 0, 255));
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

/*     if(isTest == false){
        auto result = ReadBarcode(input, opts);
        if (result.isValid()) {
            std::cout << "Original image is decodable with Code128 settings, returning as is" << std::endl;
            return input;
        }
    } */

    // Convert to grayscale and keep data alive
    std::vector<uint8_t> image = convertToGrayscale(input);
    int width = input.width();
    int height = input.height();

    // Code 128 specific parameters
    const int minModuleWidth = 1;  // More permissive minimum width
    const int maxModuleWidth = 8;  // Maximum width of a module in pixels
    const int quietZoneWidth = 10;  // Quiet zone should be 10x module width
    const double minContrast = 0.2;  // More permissive minimum contrast
    const int verticalRedundancy = static_cast<int>(height * 0.05);  // Use 5% of height for averaging

    std::cout << "Starting Code 128 enhancement with parameters:" << std::endl;
    std::cout << "- Min module width: " << minModuleWidth << std::endl;
    std::cout << "- Max module width: " << maxModuleWidth << std::endl;
    std::cout << "- Quiet zone width: " << quietZoneWidth << std::endl;
    std::cout << "- Min contrast: " << minContrast << std::endl;
    std::cout << "- Vertical redundancy: " << verticalRedundancy << std::endl;

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

} // namespace ZXing 