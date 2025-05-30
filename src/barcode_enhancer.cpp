#include "barcode_enhancer.h"
#include "zxcommon.h"
#include <cmath>

namespace ZXing {

ImageView BarcodeEnhancer::enhance1DBarcode(const ImageView& input) {
    try {
        // Convert to CImg format for processing
        CImg<uint8_t> image = convertToGrayscale(input);
        
        // Detect and correct orientation
        float angle = detectOrientation(image);
        if (std::abs(angle) > 2.0f) {
            rotateToHorizontal(image, angle);
        }
        
        // Apply Code 128 specific enhancements
        enhanceCode128(image);
        
        // Convert back to ZXing format
        return toImageView(image);
    }
    catch (const std::exception& e) {
        platform_log("Error in barcode enhancement: %s\n", e.what());
        return input;
    }
}

void BarcodeEnhancer::enhanceCode128(CImg<uint8_t>& image) {
    // 1. Initial noise reduction with edge preservation
    anisotropicSmoothing(image);
    
    // 2. Enhance vertical edges (bars)
    verticalEdgeEnhancement(image);
    
    // 3. Analyze and normalize bar widths
    int baseWidth = estimateBarWidth(image);
    normalizeBarWidths(image);
    
    // 4. Enforce Code 128 specific characteristics
    enforceBarWidthRatios(image, baseWidth);
    normalizeQuietZones(image, baseWidth);
    
    // 5. Final cleanup and binarization
    morphologicalClean(image);
    adaptiveThreshold(image);
}

void BarcodeEnhancer::anisotropicSmoothing(CImg<uint8_t>& image) {
    // Use CImg's blur_anisotropic for edge-preserving smoothing
    // Parameters tuned for vertical structures
    image.blur_anisotropic(10.0f, // amplitude
                          0.7f,   // sharpness
                          0.3f,   // anisotropy
                          0.6f,   // alpha
                          1.1f,   // sigma
                          0.8f,   // dl
                          30,     // da
                          2,      // gauss_prec
                          0,      // interpolation_type
                          true);  // fast_approx
}

void BarcodeEnhancer::verticalEdgeEnhancement(CImg<uint8_t>& image) {
    // 1. Vertical gradient using CImg's gradient
    CImg<float> gradient = image.get_gradient("y");
    
    // 2. Enhance vertical structures
    gradient.blur(0, VERTICAL_SMOOTH_RADIUS); // Blur only in vertical direction
    
    // 3. Sharpen edges
    cimg_forXY(image, x, y) {
        float edge = gradient(x, y);
        float enhanced = image(x, y) + edge * EDGE_SHARPEN_AMOUNT;
        image(x, y) = std::min(255.0f, std::max(0.0f, enhanced));
    }
}

void BarcodeEnhancer::enforceBarWidthRatios(CImg<uint8_t>& image, int baseWidth) {
    std::vector<int> widths = analyzeBarWidths(image);
    
    // Create histogram of normalized widths
    std::vector<int> hist(5, 0); // For widths 1-4 units
    for (int w : widths) {
        int normalized = std::round(static_cast<float>(w) / baseWidth);
        if (normalized > 0 && normalized <= 4) {
            hist[normalized]++;
        }
    }
    
    // Create width correction map
    std::vector<int> corrections(widths.size());
    for (size_t i = 0; i < widths.size(); i++) {
        float normalizedWidth = static_cast<float>(widths[i]) / baseWidth;
        int targetWidth = std::round(normalizedWidth);
        if (targetWidth > 0 && targetWidth <= 4) {
            corrections[i] = targetWidth * baseWidth - widths[i];
        }
    }
    
    // Apply corrections using CImg's draw_line
    int pos = 0;
    for (size_t i = 0; i < widths.size(); i++) {
        if (corrections[i] != 0) {
            int newWidth = widths[i] + corrections[i];
            image.draw_line(pos, 0, pos + newWidth - 1, image.height() - 1,
                           &image(pos, 0), 1.0f);
        }
        pos += widths[i];
    }
}

std::vector<int> BarcodeEnhancer::analyzeBarWidths(const CImg<uint8_t>& image) {
    std::vector<int> widths;
    
    // Analyze multiple scan lines for robustness
    const int numLines = 5;
    const int step = image.height() / (numLines + 1);
    
    for (int y = step; y < image.height() - step; y += step) {
        int currentWidth = 0;
        bool inBar = image(0, y) < 128;
        
        for (int x = 0; x < image.width(); x++) {
            bool isBlack = image(x, y) < 128;
            if (isBlack == inBar) {
                currentWidth++;
            } else {
                if (currentWidth > 0) {
                    widths.push_back(currentWidth);
                }
                currentWidth = 1;
                inBar = isBlack;
            }
        }
        if (currentWidth > 0) {
            widths.push_back(currentWidth);
        }
    }
    
    return widths;
}

void BarcodeEnhancer::normalizeQuietZones(CImg<uint8_t>& image, int barWidth) {
    const int requiredQuietZone = barWidth * QUIET_ZONE_MULT;
    
    // Ensure left quiet zone
    int leftQuiet = 0;
    while (leftQuiet < image.width() && image(leftQuiet, image.height()/2) > 128) {
        leftQuiet++;
    }
    if (leftQuiet < requiredQuietZone) {
        // Extend image left
        image.resize(image.width() + (requiredQuietZone - leftQuiet), image.height(),
                    1, 1, 5); // 5 = white
    }
    
    // Ensure right quiet zone
    int rightQuiet = 0;
    int x = image.width() - 1;
    while (x >= 0 && image(x, image.height()/2) > 128) {
        rightQuiet++;
        x--;
    }
    if (rightQuiet < requiredQuietZone) {
        // Extend image right
        image.resize(image.width() + (requiredQuietZone - rightQuiet), image.height(),
                    1, 1, 5); // 5 = white
    }
}

CImg<uint8_t> BarcodeEnhancer::convertToGrayscale(const ImageView& input) {
    const int width = input.width();
    const int height = input.height();
    
    CImg<uint8_t> result(width, height, 1, 1, 0);
    
    switch (input.format()) {
        case ImageFormat::Lum: {
            const uint8_t* data = input.data();
            const int stride = input.rowStride();
            cimg_forXY(result, x, y) {
                result(x, y) = data[y * stride + x];
            }
            break;
        }
        case ImageFormat::RGB:
        case ImageFormat::BGR: {
            const uint8_t* data = input.data();
            const int channels = 3;
            const int stride = input.rowStride();
            cimg_forXY(result, x, y) {
                const int offset = (y * stride + x * channels);
                // Use BT.709 luminance weights
                result(x, y) = static_cast<uint8_t>(
                    0.2126f * data[offset] +
                    0.7152f * data[offset + 1] +
                    0.0722f * data[offset + 2]
                );
            }
            break;
        }
        default:
            throw std::runtime_error("Unsupported image format");
    }
    
    return result;
}

float BarcodeEnhancer::detectOrientation(const CImg<uint8_t>& image) {
    // Use Hough transform to detect dominant lines
    CImg<uint8_t> edges = image;
    edges.blur(1.0f).sobel(0);
    
    const int diag = std::sqrt(image.width() * image.width() + 
                             image.height() * image.height());
    CImg<float> hough(180, diag, 1, 1, 0);
    
    // Accumulate votes in Hough space
    cimg_forXY(edges, x, y) {
        if (edges(x, y) > 50) { // Edge threshold
            for (int theta = 0; theta < 180; ++theta) {
                const float rad = theta * M_PI / 180.0f;
                const int rho = x * std::cos(rad) + y * std::sin(rad);
                if (rho >= 0 && rho < diag) {
                    hough(theta, rho)++;
                }
            }
        }
    }
    
    // Find peak in Hough space
    float maxVal = 0;
    int maxTheta = 0;
    cimg_forXY(hough, theta, rho) {
        if (hough(theta, rho) > maxVal) {
            maxVal = hough(theta, rho);
            maxTheta = theta;
        }
    }
    
    // Convert to angle relative to horizontal
    float angle = maxTheta - 90;
    if (angle > 90) angle -= 180;
    if (angle < -90) angle += 180;
    
    return angle;
}

void BarcodeEnhancer::rotateToHorizontal(CImg<uint8_t>& image, float angle) {
    image.rotate(angle, 2); // Bilinear interpolation
}

void BarcodeEnhancer::directionalEnhance(CImg<uint8_t>& image) {
    // Apply directional Sobel filters
    CImg<uint8_t> horizontal = image;
    CImg<uint8_t> vertical = image;
    
    applyDirectionalSobel(horizontal, false);
    applyDirectionalSobel(vertical, true);
    
    // Combine results with emphasis on vertical edges (horizontal bars)
    cimg_forXY(image, x, y) {
        float h = horizontal(x, y);
        float v = vertical(x, y);
        image(x, y) = std::min(255.0f, std::max(0.0f, v * 1.5f + h * 0.5f));
    }
}

void BarcodeEnhancer::applyDirectionalSobel(CImg<uint8_t>& image, bool vertical) {
    CImg<float> kernel(3, 3);
    if (vertical) {
        kernel.fill(1,2,1, 0,0,0, -1,-2,-1);
    } else {
        kernel.fill(1,0,-1, 2,0,-2, 1,0,-1);
    }
    image.convolve(kernel);
}

void BarcodeEnhancer::normalizeBarWidths(CImg<uint8_t>& image) {
    int barWidth = estimateBarWidth(image);
    if (barWidth < MIN_BAR_WIDTH || barWidth > MAX_BAR_WIDTH) {
        // Resize image to normalize bar width to about 4 pixels
        float scale = 4.0f / barWidth;
        int newWidth = image.width() * scale;
        int newHeight = image.height() * scale;
        image.resize(newWidth, newHeight, -100, -100, 3); // Cubic interpolation
    }
}

int BarcodeEnhancer::estimateBarWidth(const CImg<uint8_t>& image) {
    // Use run-length encoding to estimate bar width
    std::vector<int> runs;
    for (int y = 0; y < image.height(); y += image.height()/10) {
        int run = 0;
        bool inBar = false;
        for (int x = 0; x < image.width(); ++x) {
            bool isBlack = image(x, y) < 128;
            if (isBlack == inBar) {
                run++;
            } else {
                if (run > 0) runs.push_back(run);
                run = 1;
                inBar = isBlack;
            }
        }
        if (run > 0) runs.push_back(run);
    }
    
    // Use median run length as estimated bar width
    if (runs.empty()) return 4; // Default
    std::sort(runs.begin(), runs.end());
    return runs[runs.size()/2];
}

void BarcodeEnhancer::morphologicalClean(CImg<uint8_t>& image) {
    int barWidth = estimateBarWidth(image);
    
    // Create structuring elements
    CImg<uint8_t> hkernel(barWidth/2 + 1, 1, 1, 1, 255);
    CImg<uint8_t> vkernel(1, barWidth*2, 1, 1, 255);
    
    // Close operation to connect broken bars
    image.dilate(hkernel).erode(hkernel);
    
    // Open operation to remove noise
    image.erode(vkernel).dilate(vkernel);
}

void BarcodeEnhancer::enhanceLocalContrast(CImg<uint8_t>& image, int blockSize) {
    CImg<float> mean = image.get_blur(blockSize/2.0f);
    CImg<float> variance = image;
    
    // Calculate local variance
    cimg_forXY(variance, x, y) {
        float diff = image(x,y) - mean(x,y);
        variance(x,y) = diff * diff;
    }
    variance.blur(blockSize/2.0f);
    
    // Enhance based on local statistics
    cimg_forXY(image, x, y) {
        float local_std = std::sqrt(variance(x,y));
        if (local_std > 1.0f) {
            float diff = image(x,y) - mean(x,y);
            float gain = MIN_CONTRAST / local_std;
            image(x,y) = std::min(255.0f, std::max(0.0f, 
                mean(x,y) + diff * gain));
        }
    }
}

void BarcodeEnhancer::adaptiveThreshold(CImg<uint8_t>& image) {
    CImg<float> mean = image.get_blur(15.0f);
    
    // Use local mean with offset for thresholding
    cimg_forXY(image, x, y) {
        image(x,y) = (image(x,y) > mean(x,y) - 5) ? 255 : 0;
    }
}

ImageView BarcodeEnhancer::toImageView(const CImg<uint8_t>& image) {
    const int width = image.width();
    const int height = image.height();
    uint8_t* buffer = new uint8_t[width * height];
    
    cimg_forXY(image, x, y) {
        buffer[y * width + x] = image(x, y);
    }
    
    return ImageView(buffer, width, height, ImageFormat::Lum);
}

} // namespace ZXing 