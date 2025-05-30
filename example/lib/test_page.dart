import 'dart:io';
import 'package:flutter/material.dart';
import 'package:flutter_zxing/flutter_zxing.dart';
import 'package:path/path.dart' as path;

class TestPage extends StatefulWidget {
  const TestPage({super.key});

  @override
  State<TestPage> createState() => _TestPageState();
}

class _TestPageState extends State<TestPage> {
  final testImages = [
    '800446E 01XL.jpg',
    '800165E 01L.jpg',
    '2E3918D7L736.jpg',
    '257670HA64SM.jpg',
    '1H1406D40232.jpg'
  ];

  final results = <String, TestResult>{};

  @override
  void initState() {
    super.initState();
    _runTests();
  }

  Future<void> _runTests() async {
    for (final imageName in testImages) {
      final result = await _testImage(imageName);
      setState(() {
        results[imageName] = result;
      });
    }
  }

  Future<TestResult> _testImage(String imageName) async {
    try {
      // Load test image
      final imagePath = path.join('barcode_images', imageName);
      final imageFile = File(imagePath);
      if (!imageFile.existsSync()) {
        return TestResult(
          imageName: imageName,
          error: 'Image file not found',
        );
      }

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

      return TestResult(
        imageName: imageName,
        originalResult: resultOriginal,
        enhancedResult: resultEnhanced,
      );
    } catch (e) {
      return TestResult(
        imageName: imageName,
        error: e.toString(),
      );
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Barcode Enhancement Tests'),
        actions: [
          IconButton(
            icon: const Icon(Icons.refresh),
            onPressed: _runTests,
          ),
        ],
      ),
      body: ListView.builder(
        itemCount: testImages.length,
        itemBuilder: (context, index) {
          final imageName = testImages[index];
          final result = results[imageName];
          if (result == null) {
            return ListTile(
              title: Text(imageName),
              subtitle: const Text('Testing...'),
            );
          }

          if (result.error != null) {
            return ListTile(
              title: Text(imageName),
              subtitle: Text('Error: ${result.error ?? "Unknown error"}'),
              tileColor: Colors.red.shade100,
            );
          }

          final originalSuccess = result.originalResult?.isValid ?? false;
          final enhancedSuccess = result.enhancedResult?.isValid ?? false;

          Color? tileColor;
          if (!originalSuccess && enhancedSuccess) {
            tileColor = Colors.green.shade100; // Enhancement helped
          } else if (originalSuccess && !enhancedSuccess) {
            tileColor = Colors.orange.shade100; // Enhancement made it worse
          } else if (!originalSuccess && !enhancedSuccess) {
            tileColor = Colors.red.shade100; // Both failed
          }

          return Theme(
            data: Theme.of(context).copyWith(
              listTileTheme: ListTileThemeData(
                tileColor: tileColor,
              ),
            ),
            child: ExpansionTile(
              title: Text(imageName),
              subtitle: Text(
                _getResultSummary(result),
                style: TextStyle(
                  color: tileColor != null ? Colors.black87 : null,
                ),
              ),
              children: [
                if (originalSuccess)
                  ListTile(
                    title: const Text('Original Result'),
                    subtitle: Text(result.originalResult?.text ?? ''),
                  ),
                if (enhancedSuccess)
                  ListTile(
                    title: const Text('Enhanced Result'),
                    subtitle: Text(result.enhancedResult?.text ?? ''),
                  ),
                ListTile(
                  title: const Text('Original Duration'),
                  subtitle: Text('${result.originalResult?.duration ?? 0}ms'),
                ),
                ListTile(
                  title: const Text('Enhanced Duration'),
                  subtitle: Text('${result.enhancedResult?.duration ?? 0}ms'),
                ),
              ],
            ),
          );
        },
      ),
    );
  }

  String _getResultSummary(TestResult result) {
    final originalSuccess = result.originalResult?.isValid ?? false;
    final enhancedSuccess = result.enhancedResult?.isValid ?? false;

    if (!originalSuccess && enhancedSuccess) {
      return 'Enhancement improved detection!';
    } else if (originalSuccess && !enhancedSuccess) {
      return 'Enhancement degraded detection';
    } else if (originalSuccess && enhancedSuccess) {
      if (result.originalResult?.text != result.enhancedResult?.text) {
        return 'Different results detected';
      } else {
        return 'Same barcode detected';
      }
    } else {
      return 'Both methods failed';
    }
  }
}

class TestResult {
  final String imageName;
  final String? error;
  final Code? originalResult;
  final Code? enhancedResult;

  TestResult({
    required this.imageName,
    this.error,
    this.originalResult,
    this.enhancedResult,
  });
} 