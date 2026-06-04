const { getDefaultConfig } = require('expo/metro-config');

/** @type {import('expo/metro-config').MetroConfig} */
const config = getDefaultConfig(__dirname);

// 1. ASSET EXTENSION MANAGEMENT
// Ensure Metro bundles .wasm, .json database files, and common AI engine data formats
config.resolver.assetExts.push(
  'wasm',
  'json',    // Crucial for your face_database.json file!
  'onnx',    // In case your facial algorithms utilize ONNX weights
  'tflite',  // In case you bundle TensorFlow model profiles
  'bin'
);

// 2. DEFENSIVE EXPO ROUTING FOR NATIVE CODE BINDINGS
// Forces the bundler to watch changes made inside your root /cpp directory
// so your JavaScript/TypeScript re-compiles instantly when your friend modifies the C++ code.
config.watchFolders = [__dirname + '/cpp'];

// 3. BLOCK OPENCV NATIVE FOLDERS
// Metro has no reason to watch C++ native/binary files inside android/.
// The OpenCV SDK ships pre-built .so/.a libs that Windows marks as
// protected/read-only, causing EACCES crashes on startup.
config.resolver.blockList = [
  /.*\/android\/opencv\/.*/,
  /.*\/android\/.*\/src\/main\/cpp\/.*/,
  /.*\/android\/.*\.so$/,
  /.*\/android\/.*\.a$/,
];

module.exports = config;