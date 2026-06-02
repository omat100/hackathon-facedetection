#include "liveness_detector.hpp"
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <filesystem>
#include <cmath>

namespace fs = std::filesystem;

static const std::string YUNET_URL =
    "https://github.com/opencv/opencv_zoo/raw/main/models/face_detection_yunet/"
    "face_detection_yunet_2023mar.onnx";
static const std::string LBF_URL =
    "https://raw.githubusercontent.com/kurnianggoro/GSOC2017/master/data/lbfmodel.yaml";

static std::string download_file(const std::string& url, const std::string& dest) {
    if (fs::exists(dest)) return dest;
    std::cout << "Downloading " << dest << " ..." << std::endl;
    std::string cmd = "curl -k -L -o \"" + dest + "\" \"" + url + "\" 2>/dev/null";
    int ret = std::system(cmd.c_str());
    if (ret != 0 || !fs::exists(dest)) {
        std::cerr << "Failed to download " << dest << std::endl;
        return "";
    }
    std::cout << "Downloaded " << dest << std::endl;
    return dest;
}

// 68-point face landmark indices
static const std::vector<int> LEFT_EYE_IDX  = {36, 37, 38, 39, 40, 41};
static const std::vector<int> RIGHT_EYE_IDX = {42, 43, 44, 45, 46, 47};
static const int LEFT_FACE_IDX  = 0;
static const int RIGHT_FACE_IDX = 16;
static const int NOSE_TIP_IDX   = 30;
static const int LEFT_MOUTH_IDX  = 48;
static const int RIGHT_MOUTH_IDX = 54;

void LivenessDetector::ensure_models() {
    std::string yunet_path = download_file(YUNET_URL, "face_detection_yunet_2023mar.onnx");
    std::string lbf_path   = download_file(LBF_URL, "lbfmodel.yaml");

    detector_ = cv::FaceDetectorYN::create(yunet_path, "", cv::Size(320, 320), 0.6f, 0.3f, 5000);
    facemark_ = cv::face::FacemarkLBF::create();
    facemark_->loadModel(lbf_path);
}

LivenessDetector::LivenessDetector()
    : state_(LivenessState::IDLE), frame_counter_(0) {
    ensure_models();
    std::cout << "Liveness Detector ready!" << std::endl;
}

LivenessDetector::~LivenessDetector() {}

void LivenessDetector::reset() {
    state_ = LivenessState::IDLE;
    frame_counter_ = 0;
    ear_history_.clear();
}

void LivenessDetector::start() {
    reset();
    state_ = LivenessState::CHECKING;
}

std::vector<cv::Rect> LivenessDetector::detect_faces(const cv::Mat& image) {
    int h = image.rows, w = image.cols;
    detector_->setInputSize(cv::Size(w, h));
    cv::Mat faces;
    detector_->detect(image, faces);
    std::vector<cv::Rect> rects;
    for (int i = 0; i < faces.rows; i++) {
        float x1 = faces.at<float>(i, 0);
        float y1 = faces.at<float>(i, 1);
        float x2 = faces.at<float>(i, 2);
        float y2 = faces.at<float>(i, 3);
        rects.emplace_back(cv::Point(int(x1), int(y1)),
                           cv::Point(int(x2), int(y2)));
    }
    return rects;
}

float LivenessDetector::compute_ear(const std::vector<cv::Point2f>& landmarks,
                                     const std::vector<int>& eye_idx) {
    cv::Point2f p0 = landmarks[eye_idx[0]];
    cv::Point2f p1 = landmarks[eye_idx[1]];
    cv::Point2f p2 = landmarks[eye_idx[2]];
    cv::Point2f p3 = landmarks[eye_idx[3]];
    cv::Point2f p4 = landmarks[eye_idx[4]];
    cv::Point2f p5 = landmarks[eye_idx[5]];

    float v1 = cv::norm(p1 - p5);
    float v2 = cv::norm(p2 - p4);
    float h  = cv::norm(p0 - p3);
    return (v1 + v2) / (2.0f * h + 1e-6f);
}

float LivenessDetector::compute_smile_ratio(const std::vector<cv::Point2f>& landmarks) {
    cv::Point2f left_corner  = landmarks[LEFT_MOUTH_IDX];
    cv::Point2f right_corner = landmarks[RIGHT_MOUTH_IDX];
    cv::Point2f left_face    = landmarks[LEFT_FACE_IDX];
    cv::Point2f right_face   = landmarks[RIGHT_FACE_IDX];

    float mouth_width = cv::norm(right_corner - left_corner);
    float face_width  = cv::norm(right_face - left_face);
    return mouth_width / (face_width + 1e-6f);
}

float LivenessDetector::compute_head_turn_ratio(const std::vector<cv::Point2f>& landmarks) {
    cv::Point2f nose  = landmarks[NOSE_TIP_IDX];
    cv::Point2f left  = landmarks[LEFT_FACE_IDX];
    cv::Point2f right = landmarks[RIGHT_FACE_IDX];

    float to_left  = std::abs(nose.x - left.x);
    float to_right = std::abs(nose.x - right.x);
    float total    = to_left + to_right;
    return total > 0 ? to_left / total : 0.5f;
}

std::tuple<LivenessState, std::string, std::vector<cv::Point2f>>
LivenessDetector::process_frame(const cv::Mat& frame) {
    cv::Mat rgb;
    cv::cvtColor(frame, rgb, cv::COLOR_BGR2RGB);

    auto rects = detect_faces(rgb);
    if (rects.empty()) {
        return {state_, "No face detected", {}};
    }

    if (state_ != LivenessState::CHECKING) {
        std::string msg = (state_ == LivenessState::VERIFIED)
            ? "Alive!" : "Spoof detected!";
        return {state_, msg, {}};
    }

    std::vector<std::vector<cv::Point2f>> all_landmarks;
    if (!facemark_->fit(rgb, rects, all_landmarks) || all_landmarks.empty()) {
        return {state_, "No landmarks", {}};
    }

    const auto& landmarks = all_landmarks[0];
    frame_counter_++;

    if (frame_counter_ > MAX_FRAMES_) {
        state_ = LivenessState::FAILED;
        return {state_, "No movement - possible spoof!", landmarks};
    }

    float left_ear  = compute_ear(landmarks, LEFT_EYE_IDX);
    float right_ear = compute_ear(landmarks, RIGHT_EYE_IDX);
    float avg_ear   = (left_ear + right_ear) / 2.0f;
    float smile     = compute_smile_ratio(landmarks);
    float turn      = compute_head_turn_ratio(landmarks);

    // Python fallback logic (EAR history + blink/smile/turn)
    ear_history_.push_back(avg_ear);
    if ((int)ear_history_.size() > EAR_WINDOW_)
        ear_history_.pop_front();

    bool blink_detected = ((int)ear_history_.size() == EAR_WINDOW_);
    if (blink_detected) {
        for (float v : ear_history_)
            if (v >= EAR_THRESH_) { blink_detected = false; break; }
    }

    bool smile_detected = smile > SMILE_THRESH_;
    bool turn_detected  = (turn < TURN_LOW_ || turn > TURN_HIGH_);

    if (blink_detected || smile_detected || turn_detected) {
        state_ = LivenessState::VERIFIED;
        std::string action = blink_detected ? "BLINK" :
                             smile_detected ? "SMILE" : "TURN";
        return {state_, "Alive! (" + action + ")", landmarks};
    }

    return {state_, "Verifying...", landmarks};
}
