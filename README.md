# Flutter ZXing



Minimizing data copies from CameraImage to the C++ ImageView.

Implementing validatePatterns in Code128Binarizer.cpp.


validatePatterns: The no-op validatePatterns in Code128Binarizer.cpp is a key area. Implementing robust pattern validation (drawing inspiration from Java ZXing's OneDReader.recordPattern and subsequent checks) could significantly improve accuracy, reduce false positives, and potentially allow for less aggressive (faster) upstream processing.


Aha! This camera_stream.dart reveals a critical architectural choice in flutter_zxing: isolates.


The crucial link is how DecodeParams.imageFormat is set for camera images. In the example app (example/lib/main.dart's ReaderWidget._processImage):



Verify in native_zxing.cpp that the image format received from Dart (for camera frames) is indeed treated as grayscale (e.g., ImageFormat::Lum).


Flutter: CameraImage (YUV/BGRA) -> Dart Isolate -> YUV/BGRA to Grayscale/RGB conversion (likely in Dart using image package or custom logic) -> Uint8List -> Native Memory (copy) -> C++ ZXing.

Crucially, ZXing Java is designed to efficiently process YUV data directly for its binarization step, often only needing the Y (luminance) plane, avoiding a full color conversion to RGB

The image conversion step is a prime candidate for performance differences


No Explicit RGB Conversion for Camera Stream: Crucially, for camera stream processing via convertImage, there is NO conversion to RGB happening in this Dart code. It aims to send the grayscale (luminance) data to the C++ layer.

This means that in native_zxing.cpp, when ReadBarcode or ReadBarcodes is called, the imageFormat parameter passed from Dart must indicate a grayscale format (e.g., ImageFormat::Lum or ImageFormat::Gray).


Android Camera API provides a byte[] (NV21/YUV_420_888).
This byte[], along with width and height, is used to create a PlanarYUVLuminanceSource.
This source is passed to HybridBinarizer, then to the MultiFormatReader.

***

The pattern validation (validatePatterns) is currently a no-op placeholder
The Java implementation likely has more sophisticated pattern recognition algorithms

Your implementation uses LocalAverage binarizer which might not be optimal for barcodes


dm77/barcodescanner processes camera frames in a dedicated HandlerThread, which can provide more consistent performance

The native Android implementation likely benefits from hardware accelerations and optimizations specific to Android devices

dm77/barcodescanner sets parameters like setAspectTolerance(0.5f) that might be better tuned for mobile camera hardware


Train a lightweight convolutional neural network specifically to enhance Code 128 barcodes before detection

Frequency Domain Processing (FFT-based)
Code 128 barcodes have a very specific frequency signature due to their regular bar patterns. Using Fast Fourier Transform (FFT) could help:

do they relate to 
1. Frequency Domain Processing (FFT-based)
2. Specialized 1D Adaptive Thresholding for Code 128
and 3. Directional Processing Pipeline

?
Would you be able to implement them step by step and to build and run the test every time to guarantee there is no regression and track possible progress ?


Discussion on Enhancement #5: Luminance Pyramid Approach
?

Pattern-based validation (to catch and correct errors early)
Smart region selection (to improve performance and accuracy)
Adaptive parameter tuning (to handle varying image qualities)

## BUILDME
cd src; git submodule update --init --recursive

By setting ZXING_READERS to ON in the CMake configuration, we ensure that the actual implementation of the barcode reading functionality is compiled instead of the error-throwing stub.

Forcefully cleaning the build directory.
Using an absolute path for the source directory.
Explicitly specifying the generator -G "Visual Studio 17 2022".

## testME C++ Zxing
cd build ; cmake .. ; cmake --build . ; 
.\\Debug\\barcode_tests.exe

.\Release\barcode_tests.exe

./barcode_tests

The debug images are being saved as:

***

we removed partial region scanning, as it brought complexity and no gain 
we left yolo pipeline because it brought dependency nightmare, made the thing async and we did not demonstrate it led to better results

also removed of the complex enhancement techniques weren't necessary and sometimes even made things worse
Gaussian blur, Bar width correction, Quiet zone validation, Contrast enhancement, Histogram stretching, Skew detection and correction


Start with Dynamic Module Width Estimation - this requires no external libraries and will make your existing algorithms more adaptive.

Add Directional Filtering - this is a lightweight preprocessing step that will enhance the horizontal pattern of Code 128 barcodes.


other techniques to consider :

Directional Filtering: Code 128 has a specific horizontal pattern. Implementing a 1D directional filter that enhances horizontal patterns while reducing vertical noise could improve detection.
Dynamic Module Width Estimation: Your code uses fixed parameters for minimum and maximum module widths. Implementing dynamic estimation of these widths based on image analysis could make the algorithm more adaptable.
Multi-Scale Processing: Process the image at multiple scales to handle barcodes of different sizes and resolutions. This could be particularly useful for distant or small barcodes.
Machine Learning-Based Region Proposal: Use a lightweight ML model to identify potential barcode regions before detailed processing.
Frequency Domain Processing: Code 128 has a distinctive frequency signature. Using FFT (Fast Fourier Transform) to analyze and enhance the barcode pattern in the frequency domain could be effective.
Adaptive Bar Width Correction: Your correctBarWidths function exists but isn't used in the main pipeline. An enhanced version that adapts to the specific characteristics of Code 128 could be valuable.
Morphological Operations: Specific sequences of dilation and erosion operations tailored for Code 128's structure could clean up the image while preserving the critical barcode information.
Contrast Limited Adaptive Histogram Equalization (CLAHE): This could improve local contrast without amplifying noise, making barcodes more detectable in challenging lighting conditions.
Perspective Correction: Detecting and correcting perspective distortion before barcode reading could significantly improve results for non-flat barcodes.
Integration of Underused Functions: You already have several sophisticated functions like calculateSkewAngle, correctBarWidths, and validateQuietZones that aren't currently used in the main pipeline but could be valuable.

***


cd .. && git submodule update --init --recursive
cd third_party\CImg && curl -LO https://raw.githubusercontent.com/GreycLab/CImg/master/CImg.h

https://cimg.eu/

We want a bare minimum version of barcode flutterZxing Scanning that 

0. take the image I
1. enhance image with a pipeline specific to 1D Barcode I1
2. use ZXING to extract barcode info from I1
3. if no barcode detected/extracted fallback on Zxing extraction I0 



Flutter ZXing is a high-performance Flutter plugin for scanning and generating QR codes and barcodes. Built on the powerful [ZXing C++ library](https://github.com/zxing-cpp/zxing-cpp), it provides fast and reliable barcode processing capabilities for Flutter applications. Whether you need to scan barcodes from the camera or generate custom QR codes, Flutter ZXing makes it seamless and efficient.

---

## Table of Contents

- [Flutter ZXing](#flutter-zxing)
  - [BUILDME](#buildme)
  - [test C++ Zxing](#test-c-zxing)
  - [Table of Contents](#table-of-contents)
  - [Demo Screenshots](#demo-screenshots)
  - [Features](#features)
  - [Supported Formats](#supported-formats)
  - [Supported Platforms](#supported-platforms)
  - [ZXScanner](#zxscanner)
    - [Features](#features-1)
    - [Try ZXScanner](#try-zxscanner)
  - [Getting Started](#getting-started)
    - [Cloning the flutter\_zxing project](#cloning-the-flutter_zxing-project)
    - [Installing dependencies](#installing-dependencies)
    - [Use with dependency\_overrides](#use-with-dependency_overrides)
      - [Recommended Approach: Using a Git Submodule](#recommended-approach-using-a-git-submodule)
      - [Why Not Use a Direct Git Reference?](#why-not-use-a-direct-git-reference)
  - [Usage](#usage)
    - [To read barcode](#to-read-barcode)
    - [To create barcode](#to-create-barcode)
  - [License](#license)

---

## Demo Screenshots

<p align="center">
  <img alt="Scanner Screen" src="https://user-images.githubusercontent.com/11523360/222677044-a15841a7-e617-44bb-b3a0-66b2d5b57dce.png" width="240">
  <img alt="Creator Screen" src="https://user-images.githubusercontent.com/11523360/222677058-60a676fd-c229-4b51-8780-f40155cb5db6.png" width="240">
</p>
<p align="center">
  <i>Left: Barcode Scanner, Right: QR Code Creator</i>
</p>

---

## Features

- Scan QR codes and barcodes from the camera stream (on mobile platforms only), image file, or URL.
- Scan multiple barcodes at once from the camera stream (on mobile platforms only), image file, or URL.
- Generate QR codes with customizable content and size.
- Return the position points of the scanned barcode.
- Customizable scanner frame size and color, and the ability to enable or disable features like torch and pinch to zoom.

---

## Supported Formats

| Linear product | Linear industrial | Matrix             |
|----------------|-------------------|--------------------|
| UPC-A          | Code 39           | QR Code            |
| UPC-E          | Code 93           | Micro QR Code      |
| EAN-8          | Code 128          | rMQR Code          |
| EAN-13         | Codabar           | Aztec              |
| DataBar        | DataBar Expanded  | DataMatrix         |
|                | ITF               | PDF417             |
|                |                   | MaxiCode (partial) |

---

## Supported Platforms

| Platform   | Status               | Notes                                     |
|------------|----------------------|-------------------------------------------|
| Android    | ✅ Fully Supported   | Minimum API level 21                      |
| iOS        | ✅ Fully Supported   | Minimum iOS 11.0                          |
| MacOS      | ⚠️ Beta              | Without Camera support                    |
| Linux      | ⚠️ Beta              | Without Camera support                    |
| Windows    | ⚠️ Beta              | Without Camera support                    |
| Web        | ❌ Not Supported     | Dart FFI is not available on the web      |

> Note: Flutter ZXing relies on the Dart FFI feature, making it unsupported on the web. Camera-based scanning is only available on mobile platforms.

---


## ZXScanner

ZXScanner is a free QR code and barcode scanner app for Android and iOS. It is built using Flutter and the [flutter_zxing](https://github.com/khoren93/flutter_zxing) plugin.

<p align="center">
    <a href="https://github.com/khoren93/flutter_zxing/tree/main/zxscanner">
        <img src="https://user-images.githubusercontent.com/11523360/178162663-57ec28ac-7075-43ab-ac31-35058298c73e.png" alt="ZXScanner logo" height="100">
    </a>
</p>

### Features
- Fast and reliable QR code and barcode scanning.
- Built-in support for multiple barcode formats.
- Fully open-source and customizable.

### Try ZXScanner
To learn more or contribute, visit the [ZXScanner repository](https://github.com/khoren93/flutter_zxing/tree/main/zxscanner).

## Getting Started

### Cloning the flutter_zxing project

To clone the flutter_zxing project from Github which includes submodules, use the following command:

```bash
git clone --recursive https://github.com/khoren93/flutter_zxing.git
```

### Installing dependencies

Use Melos to install the dependencies of the flutter_zxing project. Melos is a tool that helps you manage multiple Dart packages in a single repository. To install Melos, use the following command:

```bash
flutter pub global activate melos
```

To install the dependencies of the flutter_zxing project, use the following command:

```bash
melos bootstrap
```

To allow the building on iOS and MacOS, you need to run the following command:

```bash
./scripts/update_ios_macos_src.sh
```

To run the integration tests:

```bash
cd example
flutter test integration_test
```

Now you can run the flutter_zxing example app on your device or emulator.

### Use with dependency_overrides

If you want to use a forked version of the flutter_zxing library in your project, you can specify it using `dependency_overrides` in your `pubspec.yaml`. However, be aware that `flutter_zxing` relies on the ZXing C++ code included as a git submodule. When using `dependency_overrides` with a git repository, these submodules are not automatically included, and the `update_ios_macos_src.sh` script is not run, which can lead to errors, especially on iOS.

Your project might build but encounter runtime errors due to missing library exports that appear as an error like this: `flutter: type 'ArgumentError' is not a subtype of type 'Code' in type cast`

#### Recommended Approach: Using a Git Submodule

To ensure all necessary files are present, it's recommended to add your forked repository as a git submodule, initialize the submodules recursively, and reference it using a local path. Follow these steps:

1. In your project add your fork as a submodule and initialize it recursively.
```bash
git submodule add https://github.com/YourUsername/flutter_zxing.git flutter_zxing
git submodule update --init --recursive
```
2. Add your submodule to your `pubspec.yaml` file as a `path` dependency override.
```yaml
dependency_overrides:
  flutter_zxing:
    path: ./flutter_zxing
```
3. Run `flutter pub get` to install the dependencies.
4. Run the `scripts/update_ios_macos_src.sh` script to update the iOS and MacOS source files. (Or add it to your own projects build process)
5. Run `flutter clean` to clear the build cache.
6. Build and run your project.

#### Why Not Use a Direct Git Reference?

Referencing your forked repo as a direct `git` reference in the `depenency_overrides` section of the `pubspec.yaml` does not include submodules or run the `update_ios_macos_src.sh` script. Manually running these steps in the `.pub-cache` directory is not practical, since the path changes with each commit.

## Usage

### To read barcode

```dart
import 'package:flutter_zxing/flutter_zxing.dart';

// Use ReaderWidget to quickly read barcode from camera image
@override
Widget build(BuildContext context) {
  return Scaffold(
    body: ReaderWidget(
      onScan: (result) async {
        // Do something with the result
      },
    ),
  );
}

// Or use flutter_zxing plugin methods 
// To read barcode from camera image directly
await zx.startCameraProcessing(); // Call this in initState

cameraController?.startImageStream((image) async {
    Code result = await zx.processCameraImage(image);
    if (result.isValid) {
        debugPrint(result.text);
    }
    return null;
});

zx.stopCameraProcessing(); // Call this in dispose

// To read barcode from XFile, String, url or Uint8List bytes
XFile xFile = XFile('Your image path');
Code? resultFromXFile = await zx.readBarcodeImagePath(xFile);

String path = 'Your local image path';
Code? resultFromPath = await zx.readBarcodeImagePathString(path);

String url = 'Your remote image url';
Code? resultFromUrl = await zx.readBarcodeImageUrl(url);

Uint8List bytes = Uint8List.fromList(yourImageBytes);
Code? resultFromBytes = await zx.readBarcode(bytes);
```

### To create barcode

```dart
import 'package:flutter_zxing/flutter_zxing.dart';
import 'dart:typed_data';
import 'package:image/image.dart' as imglib;

// Use WriterWidget to quickly create barcode
@override
Widget build(BuildContext context) {
  return Scaffold(
    body: WriterWidget(
      onSuccess: (result, bytes) {
        // Do something with the result
      },
      onError: (error) {
        // Do something with the error
      },
    ),
  );
}

// Or use FlutterZxing to create barcode directly
final Encode result = zx.encodeBarcode(
    contents: 'Text to encode',
    params: EncodeParams(
        format: Format.QRCode,
        width: 120,
        height: 120,
        margin: 10,
        eccLevel: EccLevel.low,
    ),
);
if (result.isValid && result.data != null) {
    final img = imglib.Image.fromBytes(width, height, result.data!.buffer, numChannels: 1);
    final Uint8List encodedBytes = imglib.encodePng(img);
    // use encodedBytes as you wish
}
```

## License

MIT License. See [LICENSE](https://github.com/khoren93/flutter_zxing/blob/master/LICENSE).
