#pragma once
#include <opencv2/opencv.hpp>
#include <opencv2/objdetect.hpp>
#include <opencv2/face.hpp>
#include <string>
#include <deque>

enum class LivenessState {
    IDLE = 0,
    CHECKING = 1,
    VERIFIED = 4,
    FAILED = -1
};

class LivenessDetector {
public:
    LivenessDetector();
    ~LivenessDetector();

    void reset();
    void start();
    std::tuple<LivenessState, std::string, std::vector<cv::Point2f>> process_frame(const cv::Mat& frame);

private:
    cv::Ptr<cv::FaceDetectorYN> detector_;
    cv::Ptr<cv::face::FacemarkLBF> facemark_;

    LivenessState state_;
    int frame_counter_;
    std::deque<float> ear_history_;

    static constexpr int MAX_FRAMES_ = 150;
    static constexpr int EAR_WINDOW_ = 5;
    static constexpr float EAR_THRESH_ = 0.20f;
    static constexpr float SMILE_THRESH_ = 0.48f;
    static constexpr float TURN_LOW_ = 0.35f;
    static constexpr float TURN_HIGH_ = 0.65f;

    void ensure_models();
    std::vector<cv::Rect> detect_faces(const cv::Mat& image);
    float compute_ear(const std::vector<cv::Point2f>& landmarks,
                      const std::vector<int>& eye_idx);
    float compute_smile_ratio(const std::vector<cv::Point2f>& landmarks);
    float compute_head_turn_ratio(const std::vector<cv::Point2f>& landmarks);
};
