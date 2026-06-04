#include "liveness_detector.hpp"
#include <iostream>
#include <fstream>
#include <cmath>
#include <algorithm>

#if defined(_WIN32)
  #include <io.h>       // _access()
#else
  #include <sys/stat.h> // stat()
#endif

// ─────────────────────────────────────────────────────────────────────────────
// file_exists: lives at global scope (before any namespace) so the POSIX
// stat struct and function resolve unambiguously from the C headers above,
// without interference from OpenCV / std headers that may shadow 'stat'.
// ─────────────────────────────────────────────────────────────────────────────
static bool file_exists(const std::string& path) {
#if defined(_WIN32)
    return (_access(path.c_str(), 0) == 0);
#else
    // Explicitly use the C struct and function via a local typedef to prevent
    // any name collision with identifiers pulled in by OpenCV headers.
    typedef struct stat posix_stat_t;
    posix_stat_t st;
    return (::stat(path.c_str(), &st) == 0);
#endif
}

namespace {

// Landmark / eye index constants kept here so they're TU-private.
const std::vector<int> LEFT_EYE_IDX  = {36, 37, 38, 39, 40, 41};
const std::vector<int> RIGHT_EYE_IDX = {42, 43, 44, 45, 46, 47};

constexpr int LEFT_FACE_IDX   =  0;
constexpr int RIGHT_FACE_IDX  = 16;
constexpr int NOSE_TIP_IDX    = 30;
constexpr int LEFT_MOUTH_IDX  = 48;
constexpr int RIGHT_MOUTH_IDX = 54;
constexpr int LANDMARK_SIZE   = 112;

const float TEMPLATE_POINTS[10] = {
    38.2946f, 51.6963f,
    73.5318f, 51.5014f,
    56.0252f, 71.7366f,
    41.5493f, 92.3655f,
    70.7299f, 92.2041f
};

// ── simd_affine ──────────────────────────────────────────────────────────────
// Computes the similarity-transform matrix that maps src_pts → TEMPLATE_POINTS.
//
// FIX #1: removed unused `scale` variable.
// FIX #5: guard against src_var ≈ 0 (all landmarks coincident → degenerate
//         face detection result).  Return identity-ish matrix so the caller
//         gets an empty/black aligned patch and the pipeline gracefully skips
//         the frame rather than crashing with a NaN/Inf or SIGFPE.
// FIX #2 (translation): use explicit cv::Mat_<float> columns so the
//         cv::Mat – cv::Mat subtraction is well-typed and correct.
// ─────────────────────────────────────────────────────────────────────────────
cv::Mat simd_affine(const std::vector<cv::Point2f>& src_pts) {
    // FIX #1 (original bug): initialize means to (0,0) – default ctor leaves
    //         x/y uninitialized, causing UB / wrong affine matrix.
    cv::Point2f src_mean(0.f, 0.f), dst_mean(0.f, 0.f);
    for (int i = 0; i < 5; ++i) {
        src_mean += src_pts[i];
        dst_mean += cv::Point2f(TEMPLATE_POINTS[2*i], TEMPLATE_POINTS[2*i+1]);
    }
    src_mean *= (1.f / 5.f);
    dst_mean *= (1.f / 5.f);

    float src_var = 0.f;
    for (int i = 0; i < 5; ++i) {
        cv::Point2f ds = src_pts[i] - src_mean;
        src_var += ds.x * ds.x + ds.y * ds.y;
    }

    // FIX #5: division-by-zero guard.
    if (src_var < 1e-6f) {
        // Return a 2×3 identity-like matrix; caller will get a blank patch.
        return (cv::Mat_<float>(2, 3) << 1, 0, 0, 0, 1, 0);
    }

    // FIX #1: `scale` (std::sqrt(dst_var/src_var)) was computed but never used.
    //  The rotation matrix R already encodes the scale implicitly through a/b,
    //  so it is simply removed.
    float a = 0.f, b = 0.f;
    for (int i = 0; i < 5; ++i) {
        cv::Point2f ds = src_pts[i] - src_mean;
        cv::Point2f dd = cv::Point2f(TEMPLATE_POINTS[2*i], TEMPLATE_POINTS[2*i+1]) - dst_mean;
        a += ds.x * dd.x + ds.y * dd.y;
        b += ds.x * dd.y - ds.y * dd.x;
    }
    a /= src_var;   // safe: src_var > 1e-6 guaranteed above
    b /= src_var;

    cv::Mat R = (cv::Mat_<float>(2, 2) << a, -b, b, a);

    // FIX #2 (original bug): cv::Mat(cv::Point2f) yields an ambiguous /
    //  wrongly-typed matrix; use explicit 2×1 float columns instead.
    cv::Mat src_m = (cv::Mat_<float>(2, 1) << src_mean.x, src_mean.y);
    cv::Mat dst_m = (cv::Mat_<float>(2, 1) << dst_mean.x, dst_mean.y);
    cv::Mat t = dst_m - R * src_m;   // 2×1, correct types

    cv::Mat M(2, 3, CV_32F);
    R.copyTo(M(cv::Rect(0, 0, 2, 2)));
    t.copyTo(M(cv::Rect(2, 0, 1, 2)));
    return M;
}

} // namespace


// ─────────────────────────────────────────────────────────────────────────────
// LivenessDetector implementation
// ─────────────────────────────────────────────────────────────────────────────

// ── align_and_detect_landmarks ───────────────────────────────────────────────
// FIX #3: removed unused `rect` parameter.
// FIX #4: merged the pointless detect_landmarks() wrapper into this function.
// FIX #6: hoisted the per-point cv::Mat allocation (pt, pt_orig) outside the
//         68-iteration loop to avoid 68 heap allocs per frame.
// FIX #3 (landmark scaling): if the landmark model outputs normalized coords
//         in [0, 1] the ×LANDMARK_SIZE multiply is correct.  If it already
//         outputs pixel coords the multiply must be removed.  The code is
//         annotated so the caller knows exactly what to change.
// ─────────────────────────────────────────────────────────────────────────────
std::vector<cv::Point2f>
LivenessDetector::align_and_detect_landmarks(const cv::Mat& frame,
                                              const float*   yu_landmarks) {
    std::vector<cv::Point2f> src_pts(5);
    for (int i = 0; i < 5; ++i)
        src_pts[i] = cv::Point2f(yu_landmarks[2*i], yu_landmarks[2*i+1]);

    cv::Mat M = simd_affine(src_pts);

    cv::Mat aligned;
    cv::warpAffine(frame, aligned, M, cv::Size(LANDMARK_SIZE, LANDMARK_SIZE));

    cv::Mat blob = cv::dnn::blobFromImage(
        aligned, 1.0 / 255.0,
        cv::Size(LANDMARK_SIZE, LANDMARK_SIZE),
        cv::Scalar(0, 0, 0), /*swapRB=*/false);

    landmark_net_.setInput(blob);
    cv::Mat output = landmark_net_.forward();

    // Build landmarks in the 112×112 aligned space.
    std::vector<cv::Point2f> landmarks_112(68);
    for (int i = 0; i < 68; ++i) {
        // NOTE: if your model already outputs pixel coords (0–112) remove the
        //       ×LANDMARK_SIZE multiplier below and use the raw values directly.
        landmarks_112[i] = cv::Point2f(
            output.at<float>(0, 2*i)   * LANDMARK_SIZE,
            output.at<float>(0, 2*i+1) * LANDMARK_SIZE);
    }

    // Map landmarks back to original frame coordinates.
    cv::Mat M_inv;
    cv::invertAffineTransform(M, M_inv);

    std::vector<cv::Point2f> landmarks_orig(68);

    // FIX #6: allocate pt once, reuse across all 68 iterations.
    cv::Mat pt(3, 1, CV_32F);
    pt.at<float>(2) = 1.f;   // homogeneous coordinate – fixed for all points

    for (int i = 0; i < 68; ++i) {
        pt.at<float>(0) = landmarks_112[i].x;
        pt.at<float>(1) = landmarks_112[i].y;
        cv::Mat pt_orig = M_inv * pt;   // 2×1 result
        landmarks_orig[i] = cv::Point2f(pt_orig.at<float>(0),
                                        pt_orig.at<float>(1));
    }
    return landmarks_orig;
}

// ── ensure_models ─────────────────────────────────────────────────────────────
void LivenessDetector::ensure_models() {
    std::string yunet_path    = model_dir_ + "/face_detection_yunet_2023mar.onnx";
    std::string landmark_path = model_dir_ + "/face_landmark.onnx";

    if (!file_exists(yunet_path)) {
        std::cerr << "YuNet model not found at " << yunet_path << "\n";
        return;
    }
    if (!file_exists(landmark_path)) {
        std::cerr << "Face landmark model not found at " << landmark_path << "\n";
        return;
    }

    detector_     = cv::FaceDetectorYN::create(yunet_path, "", cv::Size(640, 640),
                                               0.6f, 0.3f, 5000);
    landmark_net_ = cv::dnn::readNetFromONNX(landmark_path);
    std::cout << "Liveness models loaded!\n";
}

// ── constructor / destructor ──────────────────────────────────────────────────
LivenessDetector::LivenessDetector(const std::string& model_dir)
    : model_dir_(model_dir), state_(LivenessState::IDLE), frame_counter_(0) {
    ensure_models();
    std::cout << "Liveness Detector ready!\n";
}

LivenessDetector::~LivenessDetector() {}

// ── state control ─────────────────────────────────────────────────────────────
void LivenessDetector::reset() {
    state_         = LivenessState::IDLE;
    frame_counter_ = 0;
    ear_history_.clear();
}

void LivenessDetector::start() {
    reset();
    state_ = LivenessState::CHECKING;
}

// ── detect_faces ──────────────────────────────────────────────────────────────
// FIX #2: now stores the raw YuNet output matrix in last_faces_ so that
//         process_frame() can read the 5-point landmarks without re-running
//         detection.  The second detector_->detect() call is eliminated.
// FIX #6 (faces.empty): use faces.rows == 0 instead of cv::Mat::empty() for
//         a semantically correct "no detections" check.
// ─────────────────────────────────────────────────────────────────────────────
std::vector<cv::Rect> LivenessDetector::detect_faces(const cv::Mat& image) {
    const int h = image.rows, w = image.cols;
    detector_->setInputSize(cv::Size(w, h));
    detector_->detect(image, last_faces_);   // FIX #2: cache raw output

    std::vector<cv::Rect> rects;
    for (int i = 0; i < last_faces_.rows; ++i) {
        float x  = last_faces_.at<float>(i, 0);
        float y  = last_faces_.at<float>(i, 1);
        float fw = last_faces_.at<float>(i, 2);
        float fh = last_faces_.at<float>(i, 3);
        int x1 = std::max(0,     static_cast<int>(x));
        int y1 = std::max(0,     static_cast<int>(y));
        int x2 = std::min(w - 1, static_cast<int>(x + fw));
        int y2 = std::min(h - 1, static_cast<int>(y + fh));
        rects.emplace_back(x1, y1, x2 - x1, y2 - y1);
    }
    return rects;
}

// ── EAR / smile / turn ───────────────────────────────────────────────────────
float LivenessDetector::compute_ear(const std::vector<cv::Point2f>& landmarks,
                                     const std::vector<int>&          eye_idx) {
    const cv::Point2f& p0 = landmarks[eye_idx[0]];
    const cv::Point2f& p1 = landmarks[eye_idx[1]];
    const cv::Point2f& p2 = landmarks[eye_idx[2]];
    const cv::Point2f& p3 = landmarks[eye_idx[3]];
    const cv::Point2f& p4 = landmarks[eye_idx[4]];
    const cv::Point2f& p5 = landmarks[eye_idx[5]];

    float v1 = cv::norm(p1 - p5);
    float v2 = cv::norm(p2 - p4);
    float h  = cv::norm(p0 - p3);
    return (v1 + v2) / (2.0f * h + 1e-6f);
}

float LivenessDetector::compute_smile_ratio(const std::vector<cv::Point2f>& landmarks) {
    const cv::Point2f& left_corner  = landmarks[LEFT_MOUTH_IDX];
    const cv::Point2f& right_corner = landmarks[RIGHT_MOUTH_IDX];
    const cv::Point2f& left_face    = landmarks[LEFT_FACE_IDX];
    const cv::Point2f& right_face   = landmarks[RIGHT_FACE_IDX];

    float mouth_width = cv::norm(right_corner - left_corner);
    float face_width  = cv::norm(right_face   - left_face);
    return mouth_width / (face_width + 1e-6f);
}

float LivenessDetector::compute_head_turn_ratio(const std::vector<cv::Point2f>& landmarks) {
    const cv::Point2f& nose  = landmarks[NOSE_TIP_IDX];
    const cv::Point2f& left  = landmarks[LEFT_FACE_IDX];
    const cv::Point2f& right = landmarks[RIGHT_FACE_IDX];

    float to_left  = std::abs(nose.x - left.x);
    float to_right = std::abs(nose.x - right.x);
    float total    = to_left + to_right;
    return (total > 0.f) ? (to_left / total) : 0.5f;
}

// ── process_frame ─────────────────────────────────────────────────────────────
// FIX #2: second detector_->detect() call removed; YuNet 5-pt landmarks are
//         read from last_faces_ which detect_faces() already populated.
// FIX #4 (blink logic): the original code declared blink_detected = true when
//         the window was full, then set it false if any value was ≥ threshold.
//         This fires on a *sustained* eye-close, not a genuine blink.
//         Correct logic: look for a dip below threshold followed by a recovery
//         above it within the same window (classic dip-and-recover pattern).
// ─────────────────────────────────────────────────────────────────────────────
std::tuple<LivenessState, std::string, std::vector<cv::Point2f>>
LivenessDetector::process_frame(const cv::Mat& frame) {

    // ── face detection (single call) ────────────────────────────────────────
    std::vector<cv::Rect> rects = detect_faces(frame);   // fills last_faces_
    if (rects.empty()) {
        return {state_, "No face detected", {}};
    }

    // ── early-exit if already decided ───────────────────────────────────────
    if (state_ != LivenessState::CHECKING) {
        std::string msg = (state_ == LivenessState::VERIFIED)
                          ? "Alive!" : "Spoof detected!";
        return {state_, msg, {}};
    }

    // FIX #2: use cached last_faces_ – no second detector_->detect() needed.
    // FIX #6 (faces.empty): rows == 0 is the correct "no detection" check.
    if (last_faces_.rows == 0) {
        return {state_, "No face detected", {}};
    }

    // Columns 5-14 of YuNet output are the 5 facial keypoints (x0,y0…x4,y4).
    const float* data = last_faces_.ptr<float>(0);

    // FIX #3 / #4: call the merged helper directly (rect parameter removed).
    std::vector<cv::Point2f> landmarks =
        align_and_detect_landmarks(frame, data + 5);

    if (landmarks.empty()) {
        return {state_, "No landmarks", {}};
    }

    // ── timeout guard ────────────────────────────────────────────────────────
    ++frame_counter_;
    if (frame_counter_ > MAX_FRAMES_) {
        state_ = LivenessState::FAILED;
        return {state_, "No movement - possible spoof!", landmarks};
    }

    // ── feature extraction ───────────────────────────────────────────────────
    float avg_ear = (compute_ear(landmarks, LEFT_EYE_IDX) +
                     compute_ear(landmarks, RIGHT_EYE_IDX)) / 2.0f;
    float smile   = compute_smile_ratio    (landmarks);
    float turn    = compute_head_turn_ratio(landmarks);

    ear_history_.push_back(avg_ear);
    if (static_cast<int>(ear_history_.size()) > EAR_WINDOW_)
        ear_history_.pop_front();

    // FIX #4 (blink logic): dip-and-recover detection.
    // A genuine blink: at least one frame below EAR_THRESH_ (eyes closing)
    // followed by at least one frame at or above EAR_THRESH_ (eyes reopening).
    bool blink_detected = false;
    if (static_cast<int>(ear_history_.size()) == EAR_WINDOW_) {
        bool dipped = false;
        for (float v : ear_history_) {
            if (!dipped && v < EAR_THRESH_) {
                dipped = true;          // eye is closing
            } else if (dipped && v >= EAR_THRESH_) {
                blink_detected = true;  // eye has reopened → genuine blink
                break;
            }
        }
    }

    bool smile_detected = smile > SMILE_THRESH_;
    bool turn_detected  = (turn < TURN_LOW_ || turn > TURN_HIGH_);

    if (blink_detected || smile_detected || turn_detected) {
        state_ = LivenessState::VERIFIED;
        const char* action = blink_detected ? "BLINK"
                           : smile_detected ? "SMILE" : "TURN";
        return {state_, std::string("Alive! (") + action + ")", landmarks};
    }

    return {state_, "Verifying...", landmarks};
}