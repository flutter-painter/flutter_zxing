#pragma once

#include "zxing/core/src/ReadBarcode.h"
#include "barcode_enhancer.h"
#include "Code128Binarizer.h"
#include "zxing/core/src/ReaderOptions.h" // For Binarizer enum
#include "zxing/core/src/ThresholdBinarizer.h"
#include "zxing/core/src/GlobalHistogramBinarizer.h"
#include "zxing/core/src/HybridBinarizer.h"

namespace ZXing {

/**
 * @brief Enhanced barcode reading function that applies format-specific optimizations
 * 
 * This function wraps the standard ReadBarcode function with additional preprocessing
 * steps optimized for specific barcode formats, particularly Code 128.
 * 
 * @param input The input image
 * @param opts Reader options specifying formats and other parameters
 * @return ZXing::Result The decoded barcode result
 */
inline ZXing::Result ReadEnhancedBarcode(const ImageView& input, const ReaderOptions& opts) {
    // Check if we're only looking for Code 128
    if (opts.formats().count() == 1 && opts.hasFormat(BarcodeFormat::Code128)) {
        // Only looking for Code 128, apply multi-scale enhancement
        auto enhanced = Code128Enhancer::enhanceBarcodeMultiScale(input, opts.tryInvert());
        return ReadBarcode(enhanced, opts);
    }
    
    // For other formats or multiple formats, use standard ReadBarcode
    return ReadBarcode(input, opts);
}

/**
 * @brief Creates a bitmap with format-specific optimizations
 * 
 * This function extends the standard CreateBitmap function to use specialized
 * binarizers for specific formats like Code 128.
 * 
 * @param binarizer The binarizer type to use
 * @param iv The input image
 * @param format The barcode format (if known)
 * @return std::unique_ptr<BinaryBitmap> The resulting binary bitmap
 */
inline std::unique_ptr<BinaryBitmap> CreateEnhancedBitmap(Binarizer binarizer, const ImageView& iv, BarcodeFormat format = BarcodeFormat::None) {
    // Use specialized Code128Binarizer for Code 128 format
    if (format == BarcodeFormat::Code128) {
        return std::make_unique<Code128Binarizer>(iv);
    }
    
    // For other formats, use inlined CreateBitmap logic
    switch (binarizer) {
    case Binarizer::BoolCast: return std::make_unique<ThresholdBinarizer>(iv, 0);
    case Binarizer::FixedThreshold: return std::make_unique<ThresholdBinarizer>(iv, 127);
    case Binarizer::GlobalHistogram: return std::make_unique<GlobalHistogramBinarizer>(iv);
    case Binarizer::LocalAverage: return std::make_unique<HybridBinarizer>(iv);
    default: break; // Should not happen if binarizer is a valid enum
    }
    return {}; // Fallback, or if binarizer type is not handled
}

} // namespace ZXing
