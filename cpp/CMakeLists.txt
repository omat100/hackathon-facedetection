cmake_minimum_required(VERSION 3.14)
project(face_attendance_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs highgui videoio dnn objdetect face)
find_package(SQLite3 REQUIRED)

add_library(storage_lib STATIC storage.cpp storage.hpp)
target_link_libraries(storage_lib PRIVATE SQLite3::SQLite3)
target_include_directories(storage_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_library(face_recognition_lib STATIC face_recognition.cpp face_recognition.hpp)
target_link_libraries(face_recognition_lib PRIVATE ${OpenCV_LIBS} storage_lib)
target_include_directories(face_recognition_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_library(liveness_detector_lib STATIC liveness_detector.cpp liveness_detector.hpp)
target_link_libraries(liveness_detector_lib PRIVATE ${OpenCV_LIBS})
target_include_directories(liveness_detector_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(face_attendance main_pipeline.cpp)
target_link_libraries(face_attendance PRIVATE
    ${OpenCV_LIBS}
    SQLite3::SQLite3
    storage_lib
    face_recognition_lib
    liveness_detector_lib
)

# Keep the existing liveness_engine as well
add_executable(liveness_engine liveness_engine.cpp)
