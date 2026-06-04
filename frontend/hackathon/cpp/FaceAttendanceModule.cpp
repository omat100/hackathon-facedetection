#include <jsi/jsi.h>
#include <FrameProcessorPlugin.h>
#include <opencv2/opencv.hpp>
#include <android/log.h>

// Include your exact desktop backend headers
#include "face_recognition.hpp"
#include "liveness_detector.hpp"
#include "storage.hpp"

#define TAG "FaceAttendanceModule"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

namespace facebook::react {

class FaceAttendanceModule : public vision::FrameProcessorPlugin {
private:
    // Create static/persistent instances of your core engine modules
    FaceRecognitionEngine face_engine;
    LivenessDetector liveness_detector;
    AttendanceStorage storage;

public:
    // Make sure the internal registration string matches what we call in TypeScript
    FaceAttendanceModule() : FrameProcessorPlugin("processAttendanceFrame") {
        liveness_detector.start(); // Boot up your original tracking loop initialization
        LOGI("C++ FaceAttendanceModule fully initialized.");
    }

    jsi::Value callback(jsi::Runtime& runtime, 
                        const jsi::Value& thisValue, 
                        const jsi::Value* arguments, 
                        size_t count) override {
        
        // Defensive check: If the frame didn't pass through correctly, don't execute
        if (count == 0 || !arguments[0].isObject()) {
            return jsi::String::createFromUtf8(runtime, "Status: Frame Stream Offline");
        }

        try {
            // 1. Extract the raw Vision Camera hardware frame object
            auto frameHostObject = arguments[0].asObject(runtime).getHostObject<vision::Frame>(runtime);
            
            // 2. Fetch the hardware dimensions and raw pixel matrix memory address
            int width = frameHostObject->getWidth();
            int height = frameHostObject->getHeight();
            uint8_t* basePixelAddress = frameHostObject->getPixelBuffer();

            if (!basePixelAddress) {
                return jsi::String::createFromUtf8(runtime, "Status: Empty Pixel Allocation");
            }

            // 3. Map the raw frame buffer straight into an OpenCV matrix container
            // Vision Camera on Android natively outputs 4 channels (BGRA/RGBA) 
            cv::Mat frameMat(height, width, CV_8UC4, basePixelAddress);

            // Front-facing cameras mirror the feed natively on mobile screens. 
            // We flip it horizontally to match your desktop profile alignments
            cv::flip(frameMat, frameMat, 1);

            std::string finalUIFeedback = "Analyzing Frame Matrix...";

            // ====================================================================
            // 4. THE CORE ATTENDANCE ENGINE PIPELINE EXECUTION LOOP
            // ====================================================================
            
            // A. Trigger your original C++ Liveness Check sequence
            auto [liveness_state, liveness_msg, landmarks] = liveness_detector.process_frame(frameMat);

            // Conditional matching based on your original structural enums
            if (liveness_state == LivenessState::VERIFIED) {
                
                // B. Liveness passed (Not a spoof!) -> Run your facial recognition check
                auto [person_id, confidence, recognition_msg] = face_engine.recognize_face(frameMat);
                
                if (!person_id.empty() && person_id != "Unknown") {
                    finalUIFeedback = "Verified: " + person_id + " (" + std::to_string((int)confidence) + "%)";
                    
                    // C. Log the success entry into your local SQLite storage logic!
                    storage.log(person_id, confidence);
                } else {
                    finalUIFeedback = "Access Denied: Subject Unknown";
                }
            } 
            else if (liveness_state == LivenessState::FAILED) {
                // Catches intentional spoof attacks
                finalUIFeedback = "SPOOF ALERT: " + liveness_msg;
            } 
            else {
                // Handles non-malicious states like "No face detected" or "Keep tracking"
                finalUIFeedback = liveness_msg;
            }

            // 5. Send string response instantly back up to your React Native UI state hook
            return jsi::String::createFromUtf8(runtime, finalUIFeedback);

        } catch (const std::exception& e) {
            LOGE("Critical C++ Pipeline Catch Handler: %s", e.what());
            return jsi::String::createFromUtf8(runtime, "C++ Error: Pipeline Loop Broken");
        }
    }
};

// Hook registering our written class to the Vision Camera Proxy when the library loads
static Object registerPlugin() {
    vision::registerFrameProcessorPlugin(std::make_shared<FaceAttendanceModule>());
}

} // namespace facebook::react