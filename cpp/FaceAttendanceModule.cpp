#include <jsi/jsi.h>
#include <FrameProcessorPlugin.h>
#include <opencv2/opencv.hpp>

// Include your exact headers
#include "face_recognition.hpp"
#include "liveness_detector.hpp"
#include "storage.hpp"

using namespace facebook;

class FaceAttendanceModule : public vision::FrameProcessorPlugin {
private:
    FaceRecognitionEngine face_engine_;
    LivenessDetector liveness_;
    AttendanceStorage storage_;

public:
    FaceAttendanceModule() : FrameProcessorPlugin("processAttendanceFrame") {
        liveness_.start(); // Initialize tracking state on start
    }

    jsi::Value callback(jsi::Runtime& runtime,
                        const jsi::Value& thisValue,
                        const jsi::Value* arguments,
                        size_t count) override {

        // 1. Grab the raw mobile camera frame object from React Native
        auto frame = arguments[0].asObject(runtime).getHostObject<vision::Frame>(runtime);

        // 2. Fetch the dimensions and raw pixel pointer from the hardware buffer
        int width = frame->getWidth();
        int height = frame->getHeight();
        uint8_t* pixelBuffer = frame->getPixelBuffer();

        // 3. Wrap it cleanly into an OpenCV matrix (Most devices stream BGRA or YUV)
        cv::Mat frameMat(height, width, CV_8UC4, pixelBuffer);

        // (Optional) Match your desktop mirror effect if using front camera
        cv::flip(frameMat, frameMat, 1);

        // 4. Run your exact pipeline logic sequentially
        // A. Run Liveness First
        auto [liveness_state, liveness_msg, landmarks] = liveness_.process_frame(frameMat);

        std::string status_result = "Analyzing...";

        // Check states matching your LivenessState enum
        if (liveness_state == LivenessState::VERIFIED) {

            // B. If Liveness passes, run your Recognition Engine immediately
            auto [person_id, confidence, msg] = face_engine_.recognize_face(frameMat);

            if (!person_id.empty() && person_id != \"Unknown\") {
                status_result = "Verified: " + person_id;

                // C. Log directly to your local SQLite database
                storage_.log(person_id, confidence);
            } else {
                status_result = "Unknown Face Detected";
            }
        } else if (liveness_state == LivenessState::FAILED) {
            status_result = "Spoof Detected: " + liveness_msg;
        } else {
            status_result = "Liveness: " + liveness_msg;
        }

        // 5. Send the result text back to your React Native UI instantly
        return jsi::String::createFromUtf8(runtime, status_result);
    }
};

// Register your plugin so the React Native bundle notices it
VISION_EXPORT_FRAME_PROCESSOR_PLUGIN(FaceAttendanceModule)