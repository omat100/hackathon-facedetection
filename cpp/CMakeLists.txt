cmake_minimum_required(VERSION 3.14)
project(face_attendance_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs highgui videoio dnn objdetect face)
find_package(SQLite3 REQUIRED)

# ─── LIBRARIES ────────────────────────────────────────────────────────────────

add_library(storage_lib STATIC storage.cpp storage.hpp)
target_link_libraries(storage_lib PRIVATE SQLite3::SQLite3)
target_include_directories(storage_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_library(face_recognition_lib STATIC face_recognition.cpp face_recognition.hpp)
target_link_libraries(face_recognition_lib PRIVATE ${OpenCV_LIBS} storage_lib)
target_include_directories(face_recognition_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_library(liveness_detector_lib STATIC liveness_detector.cpp liveness_detector.hpp)
target_link_libraries(liveness_detector_lib PRIVATE ${OpenCV_LIBS})
target_include_directories(liveness_detector_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# ─── EXECUTABLES ──────────────────────────────────────────────────────────────

# Main system application pipeline
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
target_link_libraries(liveness_engine PRIVATE ${OpenCV_LIBS})

# ─── AUTOMATED WORST-CASE DEPLOYMENT STRATEGY ────────────────────────

if(WIN32)
    message(STATUS "Windows target verified. Configuring fixed post-build DLL deployment...")

    # A stable, explicitly mapped list of every required third-party library inside your folder
    set(RUNTIME_DEPENDENCIES
        "libgcc_s_seh-1.dll"
        "libstdc++-6.dll"
        "libwinpthread-1.dll"
        "libgfortran-5.dll"
        "libsqlite3-0.dll"
        "libopenblas.dll"
        "libtbb12.dll"
        "libtbbmalloc.dll"
        "libtbbmalloc_proxy.dll"
        "libopencv_core-413.dll"
        "libopencv_imgproc-413.dll"
        "libopencv_imgcodecs-413.dll"
        "libopencv_highgui-413.dll"
        "libopencv_videoio-413.dll"
        "libopencv_dnn-413.dll"
        "libopencv_objdetect-413.dll"
        "libopencv_face-413.dll"
        "libopencv_features2d-413.dll"
        "libopencv_calib3d-413.dll"
        "libopencv_flann-413.dll"
        "Qt6Core.dll"
        "Qt6Gui.dll"
        "Qt6Widgets.dll"
        "Qt6DBus.dll"
        "Qt6Network.dll"
        "Qt6OpenGL.dll"
        "Qt6OpenGLWidgets.dll"
        "Qt6PrintSupport.dll"
        "Qt6Sql.dll"
        "Qt6Test.dll"
        "Qt6Xml.dll"
    )

    set(TARGET_EXECUTABLES face_attendance liveness_engine)

    foreach(target ${TARGET_EXECUTABLES})
        # 1. Direct post-build command to copy all top-level DLLs
        foreach(dll ${RUNTIME_DEPENDENCIES})
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$ENV{MINGW_PREFIX}/bin/${dll}"
                "$<TARGET_FILE_DIR:${target}>"
                COMMENT "Deploying: ${dll}"
            )
        endforeach()

        # 2. Fixed post-build command to create the platforms folder and copy the Qt bridge natively
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target}>/platforms"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different 
                "$ENV{MINGW_PREFIX}/share/qt6/plugins/platforms/qwindows.dll" 
                "$<TARGET_FILE_DIR:${target}>/platforms/qwindows.dll"
            COMMENT "Deploying: Qt6 Windows Platform Bridge"
        )
    endforeach()
endif()