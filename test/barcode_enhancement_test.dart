import 'dart:io';
import 'package:flutter_test/flutter_test.dart';
import 'package:flutter_zxing/flutter_zxing.dart';
import 'package:path/path.dart' as path;

void main() {
  group('Barcode Enhancement Tests', () {
    final testImages = [
      '800446E 01XL.jpg',
      '800165E 01L.jpg',
      '2E3918D7L736.jpg',
      '257670HA64SM.jpg',
      '1H1406D40232.jpg'
    ];

    for (final imageName in testImages) {
      test('Test enhancement on $imageName', () async {
        // Load test image
        final imagePath = path.join('barcode_images', imageName);
        final imageFile = File(imagePath);
        expect(imageFile.existsSync(), true, reason: 'Test image should exist');

        // Read image bytes
        final imageBytes = await imageFile.readAsBytes();

        // Try reading original image
        final resultOriginal = zx.readBarcode(
          imageBytes,
          DecodeParams(
            format: Format.code128,
            tryHarder: true,
            tryRotate: true,
          ),
        );

        print('\nTesting: $imageName');
        print('Expected barcode: ${path.basenameWithoutExtension(imageName)}');
        
        if (resultOriginal.isValid) {
          print('Original detection succeeded: ${resultOriginal.text}');
        } else {
          print('Original detection failed');
        }

        // Try reading with enhancement
        final resultEnhanced = zx.readBarcode(
          imageBytes,
          DecodeParams(
            format: Format.code128,
            tryHarder: true,
            tryRotate: true,
            tryInverted: true,
            tryDownscale: true,
          ),
        );

        if (resultEnhanced.isValid) {
          print('Enhanced detection succeeded: ${resultEnhanced.text}');
        } else {
          print('Enhanced detection failed');
        }

        // Compare results
        if (!resultOriginal.isValid && resultEnhanced.isValid) {
          print('Enhancement improved detection!');
        } else if (resultOriginal.isValid && resultEnhanced.isValid) {
          if (resultOriginal.text != resultEnhanced.text) {
            print('Different results detected:');
            print('Original: ${resultOriginal.text}');
            print('Enhanced: ${resultEnhanced.text}');
          } else {
            print('Same barcode detected in both cases');
          }
        }
      });
    }
  });
} 