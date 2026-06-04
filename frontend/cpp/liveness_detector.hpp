#pragma once
#include <opencv2/opencv.hpp>
#include <opencv2/objdetect.hpp>
#include <opencv2/dnn.hpp>
#include <string>
#include <deque>

enum class LivenessState {
    IDLE     =  0,
    CHECKING =  1,
    VERIFIED =  4,
    FAILED   = -1
};

class LivenessDetector {
public:
    explicit LivenessDetector(const std::string& model_dir);
    ~LivenessDetector();

    void reset();
    void start();

    std::tuple<LivenessState, std::string, std::vector<cv::Point2f>>
        process_frame(const cv::Mat& frame);

private:
    // ── models ────────────────────────────────────────────────────────────
    cv::Ptr<cv::FaceDetectorYN> detector_;
    cv::dnn::Net                landmark_net_;
    std::string                 model_dir_;

    // FIX #2 / #redundant-detect: cache the raw YuNet output so
    // process_frame() never calls detector_->detect() a second time.
    cv::Mat last_faces_;

    // ── state ─────────────────────────────────────────────────────────────
    LivenessState      state_;
    int                frame_counter_;
    std::deque<float>  ear_history_;

    // ── tunables ──────────────────────────────────────────────────────────
    static constexpr int   MAX_FRAMES_   = 150;
    static constexpr int   EAR_WINDOW_   = 5;
    static constexpr float EAR_THRESH_   = 0.20f;
    static constexpr float SMILE_THRESH_ = 0.48f;
    static constexpr float TURN_LOW_     = 0.35f;
    static constexpr float TURN_HIGH_    = 0.65f;

    // ── helpers ───────────────────────────────────────────────────────────
    void ensure_models();

    // FIX #2: returns rects AND fills last_faces_ in one call
    std::vector<cv::Rect> detect_faces(const cv::Mat& image);

    // FIX #3 / #4: rect removed (was unused); detect_landmarks() removed
    // (was a pointless wrapper); renamed to make intent clear
    std::vector<cv::Point2f> align_and_detect_landmarks(
        const cv::Mat& frame, const float* yu_landmarks);

    float compute_ear(const std::vector<cv::Point2f>& landmarks,
                      const std::vector<int>&          eye_idx);
    float compute_smile_ratio    (const std::vector<cv::Point2f>& landmarks);
    float compute_head_turn_ratio(const std::vector<cv::Point2f>& landmarks);
};