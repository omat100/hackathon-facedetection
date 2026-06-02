#include <opencv2/opencv.hpp>
#include <iostream>
#include <string>
#include "storage.hpp"
#include "face_recognition.hpp"
#include "liveness_detector.hpp"

enum class PipelineState {
    IDLE = 0,
    LIVENESS_CHECK = 1,
    RECOGNIZING = 2,
    SUCCESS = 3,
    FAILED = -1
};

class AttendancePipeline {
public:
    AttendancePipeline()
        : face_engine_(), liveness_(), storage_(),
          state_(PipelineState::IDLE), result_timer_(0),
          RESULT_DISPLAY_FRAMES_(90), AUTO_RESTART_FRAMES_(60) {
        std::cout << "Pipeline ready!" << std::endl;
    }

    void start() {
        state_ = PipelineState::LIVENESS_CHECK;
        liveness_.start();
        result_ = "";
        result_timer_ = 0;
        std::cout << ">> Pipeline started (liveness check)" << std::endl;
    }

    std::tuple<cv::Mat, std::string, PipelineState>
    process_frame(const cv::Mat& frame) {
        cv::Mat display = frame.clone();

        if (state_ == PipelineState::IDLE) {
            draw_text(display, "Press SPACE to start", {30, 50}, {255, 255, 255});
            return {display, "idle", state_};
        }

        if (state_ == PipelineState::LIVENESS_CHECK) {
            auto [ls, msg, landmarks] = liveness_.process_frame(frame);

            cv::Scalar color(0, 165, 255);
            if (ls == LivenessState::FAILED) {
                std::cout << ">> Liveness FAILED" << std::endl;
                color = cv::Scalar(0, 0, 255);
                state_ = PipelineState::FAILED;
                result_ = "Liveness check failed!";
                result_timer_ = RESULT_DISPLAY_FRAMES_;
            } else if (ls == LivenessState::VERIFIED) {
                std::cout << ">> Liveness VERIFIED" << std::endl;
                state_ = PipelineState::RECOGNIZING;
                color = cv::Scalar(0, 255, 0);
            }

            draw_text(display, msg, {30, 50}, color);

            std::string state_name;
            switch (ls) {
                case LivenessState::IDLE:     state_name = "IDLE"; break;
                case LivenessState::CHECKING: state_name = "CHECKING"; break;
                case LivenessState::VERIFIED: state_name = "VERIFIED"; break;
                case LivenessState::FAILED:   state_name = "FAILED"; break;
            }
            draw_text(display, "Step: " + state_name, {30, 90}, {255, 255, 255});
            draw_progress(display, ls);
            return {display, result_, state_};
        }

        if (state_ == PipelineState::RECOGNIZING) {
            draw_text(display, "Recognizing...", {30, 50}, {255, 255, 0});

            auto [person_id, score, status] = face_engine_.recognize_face(frame);

            if (!person_id.empty()) {
                storage_.log(person_id, score);
                result_ = "Welcome, " + person_id + "!";
                state_ = PipelineState::SUCCESS;
                std::cout << ">> Recognized: " << person_id << " (score=" << score << ")" << std::endl;
            } else {
                result_ = "Unknown person";
                state_ = PipelineState::FAILED;
                std::cout << ">> Recognition FAILED (score=" << score << ")" << std::endl;
            }
            result_timer_ = RESULT_DISPLAY_FRAMES_;
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
                std::cout << ">> Auto-restarting pipeline..." << std::endl;
                start();
            }
            return {display, result_, state_};
        }

        if (state_ == PipelineState::FAILED) {
            draw_text(display, result_.empty() ? "Failed!" : result_,
                      {30, 50}, {0, 0, 255});
            draw_text(display, "SPACE to retry, auto-restarting...", {30, 90}, {255, 255, 255});
            result_timer_--;
            if (result_timer_ <= 0) {
                std::cout << ">> Auto-restarting after failure..." << std::endl;
                start();
            }
            return {display, result_, state_};
        }

        return {display, result_, state_};
    }

    void draw_text(cv::Mat& frame, const std::string& text,
                   cv::Point pos, cv::Scalar color) {
        cv::putText(frame, text, pos, cv::FONT_HERSHEY_SIMPLEX,
                    0.8, color, 2);
    }

    void draw_progress(cv::Mat& frame, LivenessState ls) {
        cv::Scalar color(0, 165, 255);
        if (ls == LivenessState::VERIFIED) color = cv::Scalar(0, 255, 0);
        else if (ls == LivenessState::FAILED) color = cv::Scalar(0, 0, 255);

        std::string name;
        switch (ls) {
            case LivenessState::IDLE:     name = "IDLE"; break;
            case LivenessState::CHECKING: name = "CHECKING"; break;
            case LivenessState::VERIFIED: name = "VERIFIED"; break;
            case LivenessState::FAILED:   name = "FAILED"; break;
        }
        cv::putText(frame, "Liveness: " + name, {30, 130},
                    cv::FONT_HERSHEY_SIMPLEX, 0.6, color, 2);
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
};

int main() {
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
            cv::putText(frame, "REGISTER MODE: " + register_name,
                        {30, 50}, cv::FONT_HERSHEY_SIMPLEX, 0.8,
                        cv::Scalar(0, 255, 255), 2);
            cv::putText(frame, "Press ENTER to capture",
                        {30, 90}, cv::FONT_HERSHEY_SIMPLEX, 0.6,
                        cv::Scalar(255, 255, 255), 1);
        } else {
            auto [display, result, state] = pipeline.process_frame(frame);
            frame = display;
        }

        cv::imshow("Attendance System", frame);

        int key = cv::waitKey(1) & 0xFF;
        if (key == 'q' || key == 'Q') break;
        else if (key == ' ' && !register_mode) {
            std::cout << ">> SPACE pressed - restarting" << std::endl;
            pipeline.start();
        } else if ((key == 'r' || key == 'R') && !register_mode) {
            std::cout << "Enter person name/ID: ";
            std::getline(std::cin, register_name);
            register_mode = true;
        } else if (key == 13 && register_mode) { // ENTER
            auto [ok, msg] = pipeline.register_face(frame, register_name);
            std::cout << msg << std::endl;
            register_mode = false;
            register_name = "";
        }
    }

    cap.release();
    cv::destroyAllWindows();

    auto stats = pipeline.get_storage().stats();
    std::cout << "\nTotal attendance records: " << stats.total << std::endl;

    return 0;
}
