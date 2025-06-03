#pragma once

#include "zxing/core/src/ReadBarcode.h"
#include "barcode_enhancer.h"
#include "Code128Binarizer.h"

namespace ZXing {

/**
 * @brief Enhanced barcode reading function that applies format-specific optimizations
 * 
 * This function wraps the standard ReadBarcode function with additional preprocessing
 * steps optimized for specific barcode formats, particularly Code 128.
 * 
 * @param input The input image
 * @param opts Reader options specifying formats and other parameters
 * @return Barcode The decoded barcode result
 */
inline Barcode ReadEnhancedBarcode(const ImageView& input, const ReaderOptions& opts) {
    // Check if we're only looking for Code 128
    if (opts.hasFormat(BarcodeFormat::Code128) && !opts.hasFormat(~BarcodeFormat::Code128)) {
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
        return std::make_unique<BinaryBitmap>(std::make_shared<Code128Binarizer>(iv));
    }
    
    // For other formats, use standard CreateBitmap function
    return CreateBitmap(binarizer, iv);
}

} // namespace ZXing
