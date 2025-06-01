part of 'zxing.dart';

// Getting a library that holds needed symbols
DynamicLibrary _openDynamicLibrary() {
  DynamicLibrary lib;
  try {
    if (Platform.isAndroid || Platform.isLinux) {
      lib = DynamicLibrary.open('libflutter_zxing.so');
    } else if (Platform.isWindows) {
      lib = DynamicLibrary.open('flutter_zxing.dll');
    } else {
      lib = DynamicLibrary.process();
    }
    
    // Verify that we can access the version function as a basic test
    try {
      final versionFunc = lib.lookupFunction<Pointer<Utf8> Function(), Pointer<Utf8> Function()>('version');
      final versionPtr = versionFunc();
      if (versionPtr != nullptr) {
        final version = versionPtr.toDartString();
        print('ZXing version: $version');
      } else {
        print('Error: ZXing version function returned null');
        throw Exception('build of zxing cpp does not support scanning barcodes');
      }
    } catch (e) {
      print('Error accessing ZXing version: $e');
      throw Exception('build of zxing cpp does not support scanning barcodes');
    }
    
    return lib;
  } catch (e) {
    print('Error loading ZXing library: $e');
    throw Exception('build of zxing cpp does not support scanning barcodes');
  }
}

DynamicLibrary dylib = _openDynamicLibrary();
final GeneratedBindings bindings = GeneratedBindings(dylib);
