#pragma once
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/dnn.hpp>
#include <string>

enum class LivenessState {
    IDLE     =  0,
    CHECKING =  1,
    VERIFIED =  4,
    FAILED   = -1
};

struct FaceWithLandmarks {
    cv::Rect rect;
    cv::Point2f left_eye;
    cv::Point2f right_eye;
    cv::Point2f nose;
    cv::Point2f left_mouth;
    cv::Point2f right_mouth;
};

class LivenessDetector {
public:
    // Accepts explicit path to YuNet ONNX model
    explicit LivenessDetector(const std::string& yunet_path);
    ~LivenessDetector();

    void reset();
    void start();
    std::tuple<LivenessState, std::string, std::vector<cv::Point2f>>
        process_frame(const cv::Mat& frame);

private:
    cv::dnn::Net detector_;
    std::string   yunet_path_;

    LivenessState state_;
    int           frame_counter_;

    static constexpr int   MAX_FRAMES_  = 150;
    static constexpr float SMILE_THRESH_ = 0.48f;
    static constexpr float TURN_LOW_     = 0.35f;
    static constexpr float TURN_HIGH_    = 0.65f;

    void ensure_models();
    std::vector<FaceWithLandmarks> detect_faces_with_landmarks(const cv::Mat& image);
    float compute_smile_ratio(const FaceWithLandmarks& face);
    float compute_head_turn_ratio(const FaceWithLandmarks& face, int img_w);
};
