#include "liveness_detector.hpp"
#include <iostream>
#include <cmath>
#include <algorithm>

static float iou(const cv::Rect& a, const cv::Rect& b) {
    float inter = (a & b).area();
    float uni   = a.area() + b.area() - inter;
    return inter / (uni + 1e-6f);
}

static std::vector<int> nms(const std::vector<cv::Rect>& rects,
                              const std::vector<float>& scores,
                              float threshold) {
    std::vector<int> order(scores.size());
    for (int i = 0; i < (int)scores.size(); i++) order[i] = i;
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        return scores[a] > scores[b];
    });

    std::vector<int> keep;
    std::vector<bool> suppressed(scores.size(), false);
    for (int i = 0; i < (int)order.size(); i++) {
        int idx = order[i];
        if (suppressed[idx]) continue;
        keep.push_back(idx);
        for (int j = i + 1; j < (int)order.size(); j++) {
            int jdx = order[j];
            if (suppressed[jdx]) continue;
            if (iou(rects[idx], rects[jdx]) > threshold)
                suppressed[jdx] = true;
        }
    }
    return keep;
}

// ─── Constructor ─────────────────────────────────────────────────────────────

LivenessDetector::LivenessDetector(const std::string& yunet_path)
    : yunet_path_(yunet_path),
      state_(LivenessState::IDLE),
      frame_counter_(0)
{
    ensure_models();
    std::cout << "Liveness Detector ready!" << std::endl;
}

LivenessDetector::~LivenessDetector() {}

// ─── Model Loading ────────────────────────────────────────────────────────────

void LivenessDetector::ensure_models() {
    detector_ = cv::dnn::readNetFromONNX(yunet_path_);
    if (detector_.empty()) {
        std::cerr << "Failed to load YuNet model: " << yunet_path_ << std::endl;
    }
}

// ─── State Control ────────────────────────────────────────────────────────────

void LivenessDetector::reset() {
    state_         = LivenessState::IDLE;
    frame_counter_ = 0;
}

void LivenessDetector::start() {
    reset();
    state_ = LivenessState::CHECKING;
}

// ─── Face Detection with Landmarks ───────────────────────────────────────────

std::vector<FaceWithLandmarks>
LivenessDetector::detect_faces_with_landmarks(const cv::Mat& image) {
    int h = image.rows, w = image.cols;

    cv::Mat blob = cv::dnn::blobFromImage(image, 1.0 / 127.5, cv::Size(640, 640),
                                          cv::Scalar(127.5, 127.5, 127.5), false, false);
    detector_.setInput(blob);
    cv::Mat output = detector_.forward();

    int num_detections = output.size[1];
    const float* data  = output.ptr<float>(0);

    float scale_x = (float)w / 640.0f;
    float scale_y = (float)h / 640.0f;

    std::vector<cv::Rect>  rects;
    std::vector<float>     scores;

    for (int i = 0; i < num_detections; i++) {
        const float* row = data + i * 15;
        if (row[4] < 0.5f) continue;
        cv::Rect r((int)(row[0] * scale_x), (int)(row[1] * scale_y),
                   (int)(row[2] * scale_x), (int)(row[3] * scale_y));
        rects.push_back(r);
        scores.push_back(row[4]);
    }

    std::vector<FaceWithLandmarks> all_faces;
    for (int idx : nms(rects, scores, 0.3f)) {
        const float* row = data + idx * 15;

        FaceWithLandmarks f;
        int x1 = std::max(0, (int)(row[0] * scale_x));
        int y1 = std::max(0, (int)(row[1] * scale_y));
        int x2 = std::min(w - 1, (int)((row[0] + row[2]) * scale_x));
        int y2 = std::min(h - 1, (int)((row[1] + row[3]) * scale_y));
        f.rect        = cv::Rect(x1, y1, x2 - x1, y2 - y1);
        f.left_eye    = cv::Point2f(row[5]  * scale_x, row[6]  * scale_y);
        f.right_eye   = cv::Point2f(row[7]  * scale_x, row[8]  * scale_y);
        f.nose        = cv::Point2f(row[9]  * scale_x, row[10] * scale_y);
        f.left_mouth  = cv::Point2f(row[11] * scale_x, row[12] * scale_y);
        f.right_mouth = cv::Point2f(row[13] * scale_x, row[14] * scale_y);
        all_faces.push_back(f);
    }
    return all_faces;
}

// ─── Liveness Heuristics ─────────────────────────────────────────────────────

float LivenessDetector::compute_smile_ratio(const FaceWithLandmarks& face) {
    float mouth_w = cv::norm(face.right_mouth - face.left_mouth);
    float eye_w   = cv::norm(face.right_eye   - face.left_eye);
    return mouth_w / (eye_w + 1e-6f);
}

float LivenessDetector::compute_head_turn_ratio(const FaceWithLandmarks& face, int /*img_w*/) {
    float left_x  = (float)face.rect.x;
    float right_x = (float)(face.rect.x + face.rect.width);
    float to_left  = std::abs(face.nose.x - left_x);
    float to_right = std::abs(face.nose.x - right_x);
    float total    = to_left + to_right;
    return total > 0 ? to_left / total : 0.5f;
}

// ─── Main Process Loop ────────────────────────────────────────────────────────

std::tuple<LivenessState, std::string, std::vector<cv::Point2f>>
LivenessDetector::process_frame(const cv::Mat& frame) {
    auto faces = detect_faces_with_landmarks(frame);
    if (faces.empty())
        return {state_, "No face detected", {}};

    if (state_ != LivenessState::CHECKING) {
        std::string msg = (state_ == LivenessState::VERIFIED)
            ? "Alive!" : "Spoof detected!";
        return {state_, msg, {}};
    }

    const auto& face = faces[0];
    frame_counter_++;

    if (frame_counter_ > MAX_FRAMES_) {
        state_ = LivenessState::FAILED;
        return {state_, "No movement - possible spoof!", {}};
    }

    float smile = compute_smile_ratio(face);
    float turn  = compute_head_turn_ratio(face, frame.cols);

    bool smile_detected = smile > SMILE_THRESH_;
    bool turn_detected  = (turn < TURN_LOW_ || turn > TURN_HIGH_);

    std::vector<cv::Point2f> landmarks = {
        face.left_eye, face.right_eye, face.nose, face.left_mouth, face.right_mouth
    };

    if (smile_detected || turn_detected) {
        state_ = LivenessState::VERIFIED;
        std::string action = smile_detected ? "SMILE" : "TURN";
        return {state_, "Alive! (" + action + ")", landmarks};
    }

    return {state_, "Verifying...", landmarks};
}
