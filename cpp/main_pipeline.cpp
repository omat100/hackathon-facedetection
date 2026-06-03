#include <opencv2/opencv.hpp>
#include <iostream>
#include <string>
#include "storage.hpp"
#include "face_recognition.hpp"
#include "liveness_detector.hpp"

enum class PipelineState {
    IDLE = 0,
    PROCESSING = 1, // Combined active analysis state
    SUCCESS = 3,
    FAILED = -1
};

class AttendancePipeline {
public:
    AttendancePipeline()
        : face_engine_(), liveness_(), storage_(),
          state_(PipelineState::IDLE), result_timer_(0),
          RESULT_DISPLAY_FRAMES_(90), AUTO_RESTART_FRAMES_(60),
          liveness_passed_(false), face_recognized_(false) {
        std::cout << "Pipeline ready!" << std::endl;
    }

    void start() {
        state_ = PipelineState::PROCESSING;
        liveness_.start();
        result_ = "";
        result_timer_ = 0;
        liveness_passed_ = false;
        face_recognized_ = false;
        recognized_person_ = "";
        last_recognition_score_ = 0.0;
        std::cout << ">> Pipeline started (Simultaneous Liveness + Recognition tracking active)" << std::endl;
    }

    std::tuple<cv::Mat, std::string, PipelineState>
    process_frame(const cv::Mat& frame) {
        cv::Mat display = frame.clone();

        if (state_ == PipelineState::IDLE) {
            draw_text(display, "Press SPACE to start", {30, 50}, {255, 255, 255});
            return {display, "idle", state_};
        }

        if (state_ == PipelineState::PROCESSING) {
            // 1. Run Liveness Detection Check
            auto [ls, liveness_msg, landmarks] = liveness_.process_frame(frame);

            // Update liveness status if it hasn't passed yet
            if (!liveness_passed_) {
                if (ls == LivenessState::VERIFIED) {
                    liveness_passed_ = true;
                    std::cout << ">> Liveness verified via active frame analysis!" << std::endl;
                } else if (ls == LivenessState::FAILED) {
                    std::cout << ">> Liveness processing reached absolute timeout." << std::endl;
                    state_ = PipelineState::FAILED;
                    result_ = "Liveness check failed!";
                    result_timer_ = RESULT_DISPLAY_FRAMES_;
                    return {display, result_, state_};
                }
            }

            // 2. Run Face Recognition (Parallel execution pass on the same frame matrix)
            if (!face_recognized_) {
                auto [person_id, score, status] = face_engine_.recognize_face(frame, 0.32); // Lowered threshold filter
                last_recognition_score_ = score;
                
                if (!person_id.empty()) {
                    face_recognized_ = true;
                    recognized_person_ = person_id;
                    std::cout << ">> Face matched successfully: " << person_id << " (score: " << score << ")" << std::endl;
                }
            }

            // 3. Render real-time overlay graphics to screen layout
            cv::Scalar liveness_color = liveness_passed_ ? cv::Scalar(0, 255, 0) : cv::Scalar(0, 165, 255);
            std::string liveness_ui_msg = liveness_passed_ ? "Liveness: ALIVE!" : liveness_msg;
            draw_text(display, liveness_ui_msg, {30, 50}, liveness_color);

            cv::Scalar rec_color = face_recognized_ ? cv::Scalar(0, 255, 0) : cv::Scalar(0, 255, 255);
            std::string rec_ui_msg = face_recognized_ ? "Identity: " + recognized_person_ : "Identity: Scanning...";
            draw_text(display, rec_ui_msg, {30, 90}, rec_color);

            // 4. State transition handling once both criteria match
            if (liveness_passed_ && face_recognized_) {
                storage_.log(recognized_person_, last_recognition_score_);
                result_ = "Welcome, " + recognized_person_ + "!";
                state_ = PipelineState::SUCCESS;
                result_timer_ = RESULT_DISPLAY_FRAMES_;
            }

            return {display, result_, state_};
        }

        if (state_ == PipelineState::SUCCESS) {
            draw_text(display, result_, {30, 50}, {0, 255, 0});
            draw_text(display, "Attendance logged!", {30, 90}, {0, 255, 0});
            result_timer_--;
            if (result_timer_ <= AUTO_RESTART_FRAMES_ && result_timer_ > 0) {
                draw_text(display, "Auto-restarting...", {30, 130}, {255, 255, 255});
            }
            if (result_timer_ <= 0) {
                start();
            }
            return {display, result_, state_};
        }

        if (state_ == PipelineState::FAILED) {
            draw_text(display, result_.empty() ? "Failed!" : result_, {30, 50}, {0, 0, 255});
            draw_text(display, "SPACE to retry, auto-restarting...", {30, 90}, {255, 255, 255});
            result_timer_--;
            if (result_timer_ <= 0) {
                start();
            }
            return {display, result_, state_};
        }

        return {display, result_, state_};
    }

    void draw_text(cv::Mat& frame, const std::string& text, cv::Point pos, cv::Scalar color) {
        cv::putText(frame, text, pos, cv::FONT_HERSHEY_SIMPLEX, 0.8, color, 2);
    }

    AttendanceStorage& get_storage() { return storage_; }
    std::pair<bool, std::string> register_face(const cv::Mat& image, const std::string& person_id) {
        return face_engine_.register_face(image, person_id);
    }

private:
    FaceRecognitionEngine face_engine_;
    LivenessDetector liveness_;
    AttendanceStorage storage_;
    PipelineState state_;
    std::string result_;
    int result_timer_;
    const int RESULT_DISPLAY_FRAMES_;
    const int AUTO_RESTART_FRAMES_;
    
    // Independent parallel monitoring variables
    bool liveness_passed_;
    bool face_recognized_;
    std::string recognized_person_;
    double last_recognition_score_;
};

int main() {
    // Keep your exact original main loop initialization logic 
    AttendancePipeline pipeline;
    cv::VideoCapture cap(0);
    if (!cap.isOpened()) {
        std::cerr << "Failed to open camera" << std::endl;
        return 1;
    }

    std::cout << "\n=== ATTENDANCE PIPELINE ===" << std::endl;
    std::cout << "SPACE - Start / restart anytime" << std::endl;
    std::cout << "R     - Register new face" << std::endl;
    std::cout << "Q     - Quit" << std::endl;

    bool register_mode = false;
    std::string register_name;

    cv::Mat frame;
    while (cap.read(frame)) {
        cv::flip(frame, frame, 1);

        if (register_mode) {
            cv::putText(frame, "REGISTER MODE: " + register_name, {30, 50}, cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 255, 255), 2);
            cv::putText(frame, "Press ENTER to capture", {30, 90}, cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 255), 1);
        } else {
            auto [display, result, state] = pipeline.process_frame(frame);
            frame = display;
        }

        cv::imshow("Attendance System", frame);

        int key = cv::waitKey(1) & 0xFF;
        if (key == 'q' || key == 'Q') break;
        else if (key == ' ' && !register_mode) {
            pipeline.start();
        } else if ((key == 'r' || key == 'R') && !register_mode) {
            std::cout << "Enter person name/ID: ";
            std::getline(std::cin, register_name);
            register_mode = true;
        } else if (key == 13 && register_mode) { 
            auto [ok, msg] = pipeline.register_face(frame, register_name);
            std::cout << msg << std::endl;
            register_mode = false;
            register_name = "";
        }
    }

    cap.release();
    cv::destroyAllWindows();
    return 0;
}