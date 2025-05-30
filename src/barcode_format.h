#pragma once

namespace ZXing {
namespace Format {

// These values must match the Format class in lib/src/models/format.dart
enum : int {
    NONE = 0,
    AZTEC = 1 << 0,
    CODABAR = 1 << 1,
    CODE_39 = 1 << 2,
    CODE_93 = 1 << 3,
    CODE_128 = 1 << 4,
    DATABAR = 1 << 5,
    DATABAR_EXPANDED = 1 << 6,
    DATA_MATRIX = 1 << 7,
    EAN_8 = 1 << 8,
    EAN_13 = 1 << 9,
    ITF = 1 << 10,
    MAXICODE = 1 << 11,
    PDF417 = 1 << 12,
    QR_CODE = 1 << 13,
    UPC_A = 1 << 14,
    UPC_E = 1 << 15,
    MICRO_QR_CODE = 1 << 16,
    RMQR_CODE = 1 << 17,

    // Composite flags
    LINEAR_CODES = CODABAR | CODE_39 | CODE_93 | CODE_128 | EAN_8 | EAN_13 | 
                  ITF | DATABAR | DATABAR_EXPANDED | UPC_A | UPC_E,
                  
    MATRIX_CODES = AZTEC | DATA_MATRIX | MAXICODE | PDF417 | QR_CODE | 
                  MICRO_QR_CODE | RMQR_CODE,
                  
    ANY = LINEAR_CODES | MATRIX_CODES
};

} // namespace Format
} // namespace ZXing 