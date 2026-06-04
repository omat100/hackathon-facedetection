Pod::Spec.new do |s|
  s.name         = "FaceAttendanceModule"
  s.version      = "1.0.0"
  s.summary      = "Bridges OpenCV C++ attendance code into iOS"
  s.homepage     = "https://your-homepage.com"
  s.license      = "MIT"
  s.authors      = { "Your Name" => "your-email@example.com" }
  s.platforms    = { :ios => "16.4" }
  s.source       = { :path => '.' }

  s.source_files = "*.{h,m,mm}", "cpp/**/*.{h,hpp,cpp}"
  s.public_header_files = "*.h", "cpp/**/*.hpp"

  # FIX: include .onnx files from root AND cpp/ subfolder so either
  # location works. Also bundle face_database.json from root.
  s.resources = "*.onnx", "face_database.json", "cpp/*.onnx", "cpp/face_database.json"

  s.vendored_frameworks = "opencv2.framework"

  s.dependency "React-Core"
  s.dependency "VisionCamera"
  s.libraries    = "sqlite3"

  s.xcconfig = {
    'CLANG_CXX_LANGUAGE_STANDARD' => 'c++17',
    'CLANG_CXX_LIBRARY' => 'libc++',
    'HEADER_SEARCH_PATHS' => '"$(PODS_TARGET_SRCROOT)" "$(PODS_TARGET_SRCROOT)/cpp"'
  }
end
