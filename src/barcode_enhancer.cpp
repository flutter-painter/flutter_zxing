#include "barcode_enhancer.h"
#include <cmath>
#include <algorithm>
#include <memory>
#include <numeric>
#include <cstring>
#include <stb_image_write.h>

namespace ZXing {

std::vector<uint8_t> BarcodeEnhancer::convertToGrayscale(const ImageView& input) {
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

void BarcodeEnhancer::gaussianBlur(std::vector<uint8_t>& image, int width, int height, float sigma) {
    // Create Gaussian kernel
    int kernelSize = static_cast<int>(std::ceil(sigma * 6));
    if (kernelSize % 2 == 0) kernelSize++;
    
    std::vector<float> kernel(kernelSize);
    float sum = 0.0f;
    int center = kernelSize / 2;
    
    for (int i = 0; i < kernelSize; i++) {
        float x = static_cast<float>(i - center);
        kernel[i] = std::exp(-(x * x) / (2 * sigma * sigma));
        sum += kernel[i];
    }
    
    // Normalize kernel
    for (float& k : kernel) {
        k /= sum;
    }
    
    // Apply horizontal blur
    std::vector<uint8_t> temp(width * height);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float sum = 0.0f;
            float weightSum = 0.0f;
            
            for (int k = -center; k <= center; k++) {
                int px = x + k;
                if (px >= 0 && px < width) {
                    float weight = kernel[k + center];
                    sum += image[y * width + px] * weight;
                    weightSum += weight;
                }
            }
            
            temp[y * width + x] = static_cast<uint8_t>(sum / weightSum);
        }
    }
    
    // Apply vertical blur
    for (int x = 0; x < width; x++) {
        for (int y = 0; y < height; y++) {
            float sum = 0.0f;
            float weightSum = 0.0f;
            
            for (int k = -center; k <= center; k++) {
                int py = y + k;
                if (py >= 0 && py < height) {
                    float weight = kernel[k + center];
                    sum += temp[py * width + x] * weight;
                    weightSum += weight;
                }
            }
            
            image[y * width + x] = static_cast<uint8_t>(sum / weightSum);
        }
    }
}

void BarcodeEnhancer::adaptiveThreshold(std::vector<uint8_t>& image, int width, int height, int windowSize, float C) {
    std::vector<uint8_t> temp = image;
    int halfWindow = windowSize / 2;
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // Calculate local mean
            int count = 0;
            int sum = 0;
            
            for (int wy = -halfWindow; wy <= halfWindow; wy++) {
                int py = y + wy;
                if (py < 0 || py >= height) continue;
                
                for (int wx = -halfWindow; wx <= halfWindow; wx++) {
                    int px = x + wx;
                    if (px < 0 || px >= width) continue;
                    
                    sum += temp[py * width + px];
                    count++;
                }
            }
            
            float mean = static_cast<float>(sum) / count;
            uint8_t threshold = static_cast<uint8_t>(mean - C);
            image[y * width + x] = (temp[y * width + x] < threshold) ? 0 : 255;
        }
    }
}

void BarcodeEnhancer::erode(std::vector<uint8_t>& image, int width, int height, int kernelSize) {
    std::vector<uint8_t> temp = image;
    int halfKernel = kernelSize / 2;
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint8_t minVal = 255;
            
            for (int ky = -halfKernel; ky <= halfKernel; ky++) {
                int py = y + ky;
                if (py < 0 || py >= height) continue;
                
                for (int kx = -halfKernel; kx <= halfKernel; kx++) {
                    int px = x + kx;
                    if (px < 0 || px >= width) continue;
                    
                    minVal = std::min(minVal, temp[py * width + px]);
                }
            }
            
            image[y * width + x] = minVal;
        }
    }
}

void BarcodeEnhancer::dilate(std::vector<uint8_t>& image, int width, int height, int kernelSize) {
    std::vector<uint8_t> temp = image;
    int halfKernel = kernelSize / 2;
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint8_t maxVal = 0;
            
            for (int ky = -halfKernel; ky <= halfKernel; ky++) {
                int py = y + ky;
                if (py < 0 || py >= height) continue;
                
                for (int kx = -halfKernel; kx <= halfKernel; kx++) {
                    int px = x + kx;
                    if (px < 0 || px >= width) continue;
                    
                    maxVal = std::max(maxVal, temp[py * width + px]);
                }
            }
            
            image[y * width + x] = maxVal;
        }
    }
}

ImageView BarcodeEnhancer::toImageView(const std::vector<uint8_t>& image, int width, int height) {
    // Create a copy of the image data that will be owned by the ImageView
    uint8_t* data = new uint8_t[width * height];
    std::memcpy(data, image.data(), width * height);
    
    // Return ImageView with the copied data
    return ImageView(data, width, height, ImageFormat::Lum);
}

void saveDebugImage(const std::vector<uint8_t>& image, int width, int height, const char* filename) {
    stbi_write_png(filename, width, height, 1, image.data(), width);
}

ImageView BarcodeEnhancer::enhance1DBarcode(const ImageView& input) {
    // Convert to grayscale if needed
    std::vector<uint8_t> image = convertToGrayscale(input);
    int width = input.width();
    int height = input.height();

    // Save original grayscale
    saveDebugImage(image, width, height, "debug_1_grayscale.png");

    // Enhance contrast with gentler parameters
    stretchHistogram(image, 2.0f, 98.0f); // Less aggressive stretch
    saveDebugImage(image, width, height, "debug_2_stretched.png");

    // Normalize with gentler contrast enhancement
    normalizeImage(image);
    saveDebugImage(image, width, height, "debug_3_normalized.png");

    // Apply Gaussian blur with smaller sigma
    gaussianBlur(image, width, height, 0.8f);
    saveDebugImage(image, width, height, "debug_4_blurred.png");

    // Apply adaptive thresholding with larger window and smaller C
    adaptiveThreshold(image, width, height, 31, 3.0f);
    saveDebugImage(image, width, height, "debug_5_thresholded.png");

    // Morphological operations with smaller kernels
    erode(image, width, height, 2);
    saveDebugImage(image, width, height, "debug_6_eroded.png");
    
    dilate(image, width, height, 2);
    saveDebugImage(image, width, height, "debug_7_dilated.png");

    return toImageView(image, width, height);
}

void BarcodeEnhancer::enhanceContrast(std::vector<uint8_t>& image) {
    if (image.empty()) return;

    // Calculate mean and standard deviation
    double sum = 0.0;
    double sumSq = 0.0;
    for (uint8_t pixel : image) {
        sum += pixel;
        sumSq += pixel * pixel;
    }
    double mean = sum / image.size();
    double variance = (sumSq / image.size()) - (mean * mean);
    double stdDev = std::sqrt(variance);

    // Apply contrast enhancement
    double alpha = 2.0; // Contrast factor
    for (auto& pixel : image) {
        double normalized = (pixel - mean) / stdDev;
        double enhanced = mean + (normalized * alpha * stdDev);
        pixel = static_cast<uint8_t>(std::clamp(enhanced, 0.0, 255.0));
    }
}

void BarcodeEnhancer::normalizeImage(std::vector<uint8_t>& image) {
    if (image.empty()) return;

    // Find min and max values
    uint8_t minVal = *std::min_element(image.begin(), image.end());
    uint8_t maxVal = *std::max_element(image.begin(), image.end());

    if (maxVal == minVal) return;

    // Normalize to full range [0, 255]
    for (auto& pixel : image) {
        pixel = static_cast<uint8_t>((static_cast<float>(pixel - minVal) / (maxVal - minVal)) * 255);
    }
}

void BarcodeEnhancer::stretchHistogram(std::vector<uint8_t>& image, float lowPercentile, float highPercentile) {
    if (image.empty()) return;

    // Create histogram
    std::vector<int> histogram(256, 0);
    for (uint8_t pixel : image) {
        histogram[pixel]++;
    }

    // Calculate cumulative histogram
    std::vector<int> cumulative(256, 0);
    cumulative[0] = histogram[0];
    for (int i = 1; i < 256; i++) {
        cumulative[i] = cumulative[i-1] + histogram[i];
    }

    // Find percentile values
    int totalPixels = image.size();
    int lowCount = static_cast<int>((lowPercentile / 100.0f) * totalPixels);
    int highCount = static_cast<int>((highPercentile / 100.0f) * totalPixels);

    uint8_t lowValue = 0;
    uint8_t highValue = 255;

    for (int i = 0; i < 256; i++) {
        if (cumulative[i] >= lowCount) {
            lowValue = static_cast<uint8_t>(i);
            break;
        }
    }

    for (int i = 255; i >= 0; i--) {
        if (cumulative[i] <= highCount) {
            highValue = static_cast<uint8_t>(i);
            break;
        }
    }

    // Apply histogram stretch
    float scale = 255.0f / (highValue - lowValue);
    for (auto& pixel : image) {
        if (pixel <= lowValue) {
            pixel = 0;
        } else if (pixel >= highValue) {
            pixel = 255;
        } else {
            pixel = static_cast<uint8_t>((pixel - lowValue) * scale);
        }
    }
}

std::vector<uint8_t> BarcodeEnhancer::createKernel(int size) {
    std::vector<uint8_t> kernel(size * size, 1);
    return kernel;
}

} // namespace ZXing 