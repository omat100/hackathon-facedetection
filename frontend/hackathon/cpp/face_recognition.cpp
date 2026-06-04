#include "face_recognition.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstdlib>
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

// YuNet face detection model
static const std::string YUNET_URL =
    "https://github.com/opencv/opencv_zoo/raw/main/models/face_detection_yunet/"
    "face_detection_yunet_2023mar.onnx";

// SFace recognition model (OpenCV Zoo — already bundled in the project)
static const std::string SFACE_MODEL_NAME = "face_recognition_sface_2021dec.onnx";

// ArcFace/InsightFace model download URLs (fallback if SFace not found)
static const std::string ARCFACE_HF_URL =
    "https://huggingface.co/deepinsight/insightface/resolve/main/models/"
    "buffalo_sc/w600k_mbf.onnx";
static const std::string ARCFACE_GH_ZIP_SC =
    "https://github.com/deepinsight/insightface/releases/download/v0.7/buffalo_sc.zip";
static const std::string ARCFACE_GH_ZIP_L =
    "https://github.com/deepinsight/insightface/releases/download/v0.7/buffalo_l.zip";

// 112x112 reference landmarks for similarity alignment
static const float ARC_REF_LM[5][2] = {
    {38.2946f, 51.6963f},  // left eye
    {73.5318f, 51.5014f},  // right eye
    {56.0252f, 71.7366f},  // nose tip
    {41.5493f, 92.3655f},  // left mouth corner
    {70.7299f, 92.2041f},  // right mouth corner
};

std::string FaceRecognitionEngine::download_file(const std::string& url, const std::string& dest) {
    if (fs::exists(dest)) return dest;
    std::cout << "Downloading " << dest << " ..." << std::endl;
    
    // Convert path format to match the native operating system slashes
    std::string native_dest = fs::path(dest).make_preferred().string();
    std::string cmd;

#if defined(_WIN32) || defined(WIN32)
    // Windows-friendly: Uses native curl, bypasses SSL verification issues with -k,
    // and suppresses the download progress bars cleanly via quiet/silent flags (-s -S)
    cmd = "curl -k -L -s -S -o \"" + native_dest + "\" \"" + url + "\"";
#else
    // Mac/Linux-friendly: Safely redirects standard error to /dev/null
    cmd = "curl -k -L -o \"" + native_dest + "\" \"" + url + "\" 2>/dev/null";
#endif

    int ret = std::system(cmd.c_str());
    if (ret != 0 || !fs::exists(dest)) {
        std::cerr << "Failed to download " << dest << std::endl;
        return "";
    }
    std::cout << "Downloaded " << dest << std::endl;
    return dest;
}

static std::string find_recognition_model() {
    // Prefer already-downloaded SFace model from the project
    if (fs::exists(SFACE_MODEL_NAME) && fs::file_size(SFACE_MODEL_NAME) > 1000000) {
        std::cout << "Using existing " << SFACE_MODEL_NAME << std::endl;
        return SFACE_MODEL_NAME;
    }

    // Fallback: try the old model name
    const std::string name = "w600k_mbf.onnx";
    if (fs::exists(name)) return name;

    // Try 1: HuggingFace direct download
    std::cout << "Downloading " << name << " from HuggingFace..." << std::endl;
    std::string cmd = "curl -k -L -o \"" + name + "\" \"" + ARCFACE_HF_URL + "\" 2>/dev/null";
    if (std::system(cmd.c_str()) == 0 && fs::exists(name) && fs::file_size(name) > 1000000) {
        std::cout << "Downloaded " << name << std::endl;
        return name;
    }
    std::remove(name.c_str());

    // Try 2: Extract from buffalo_sc.zip (GitHub)
    std::cout << "Downloading buffalo_sc.zip (14MB)..." << std::endl;
    {
        std::string zip = "buffalo_sc.zip";
        cmd = "curl -k -L -o \"" + zip + "\" \"" + ARCFACE_GH_ZIP_SC + "\" 2>/dev/null";
        if (std::system(cmd.c_str()) == 0 && fs::exists(zip)) {
            std::system(("unzip -o \"" + zip + "\" \"" + name + "\" 2>/dev/null").c_str());
            std::remove(zip.c_str());
            if (fs::exists(name)) {
                std::cout << "Downloaded " << name << std::endl;
                return name;
            }
        }
    }

    std::cerr << "Failed to load recognition model!" << std::endl;
    return "";
}

void FaceRecognitionEngine::ensure_models() {
    std::string yunet_path = download_file(YUNET_URL, "face_detection_yunet_2023mar.onnx");

    // YuNet detector with 640x640 input for better detection
    detector_ = cv::FaceDetectorYN::create(yunet_path, "", cv::Size(640, 640), 0.5f, 0.3f, 5000);

    // Load recognition model (prefer local SFace, fallback to ArcFace download)
    std::string model_path = find_recognition_model();
    if (model_path.empty() || !fs::exists(model_path)) {
        std::cerr << "Recognition model not found!" << std::endl;
        return;
    }
    arcface_net_ = cv::dnn::readNetFromONNX(model_path);
    if (arcface_net_.empty()) {
        std::cerr << "Failed to parse ONNX model: " << model_path << std::endl;
        return;
    }
    arcface_net_.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
    arcface_net_.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);

    // SFace outputs 128-dim, ArcFace outputs 512-dim
    expected_dim_ = (model_path == SFACE_MODEL_NAME) ? 128 : 512;
    std::cout << "Loaded recognition model: " << model_path
              << " (dim=" << expected_dim_ << ")" << std::endl;
}

void FaceRecognitionEngine::apply_clahe(cv::Mat& frame) {
    cv::Mat lab;
    cv::cvtColor(frame, lab, cv::COLOR_BGR2Lab);
    std::vector<cv::Mat> channels;
    cv::split(lab, channels);
    cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(2.0, cv::Size(8, 8));
    clahe->apply(channels[0], channels[0]);
    cv::merge(channels, lab);
    cv::cvtColor(lab, frame, cv::COLOR_Lab2BGR);
}

FaceRecognitionEngine::FaceRecognitionEngine() : expected_dim_(0) {
    ensure_models();
    load_database();
    std::cout << "Face Recognition Engine ready! (dim=" << expected_dim_ << ")" << std::endl;
}

FaceRecognitionEngine::~FaceRecognitionEngine() {}

std::vector<cv::Mat> FaceRecognitionEngine::detect_faces(const cv::Mat& image) {
    int h = image.rows, w = image.cols;
    detector_->setInputSize(cv::Size(w, h));
    cv::Mat faces;
    detector_->detect(image, faces);
    std::vector<cv::Mat> result;
    for (int i = 0; i < faces.rows; i++) {
        result.push_back(faces.row(i).clone());
    }
    return result;
}

cv::Mat FaceRecognitionEngine::align_face(const cv::Mat& image, const cv::Mat& face) {
    // Extract 5 landmarks from FaceDetectorYN output
    std::vector<cv::Point2f> src(5), dst(5);
    src[0] = cv::Point2f(face.at<float>(0, 5), face.at<float>(0, 6));   // left eye
    src[1] = cv::Point2f(face.at<float>(0, 7), face.at<float>(0, 8));   // right eye
    src[2] = cv::Point2f(face.at<float>(0, 9), face.at<float>(0, 10));  // nose
    src[3] = cv::Point2f(face.at<float>(0, 11), face.at<float>(0, 12)); // left mouth
    src[4] = cv::Point2f(face.at<float>(0, 13), face.at<float>(0, 14)); // right mouth

    for (int i = 0; i < 5; i++) {
        dst[i] = cv::Point2f(ARC_REF_LM[i][0], ARC_REF_LM[i][1]);
    }

    // Estimate similarity transform (rotation + scale + translation)
    cv::Mat tform = cv::estimateAffinePartial2D(src, dst);
    if (tform.empty()) {
        // Fallback: estimate full affine
        tform = cv::getAffineTransform(src.data(), dst.data());
    }

    cv::Mat aligned;
    cv::warpAffine(image, aligned, tform, cv::Size(112, 112));
    return aligned;
}

int select_largest_face(const std::vector<cv::Mat>& faces) {
    if (faces.empty()) return -1;
    int best_idx = 0;
    float best_area = 0;
    for (int i = 0; i < (int)faces.size(); i++) {
        float x1 = faces[i].at<float>(0, 0);
        float y1 = faces[i].at<float>(0, 1);
        float x2 = faces[i].at<float>(0, 2);
        float y2 = faces[i].at<float>(0, 3);
        float area = (x2 - x1) * (y2 - y1);
        if (area > best_area) {
            best_area = area;
            best_idx = i;
        }
    }
    return best_idx;
}

cv::Mat FaceRecognitionEngine::get_embedding(const cv::Mat& aligned_face) {
    if (arcface_net_.empty()) {
        std::cerr << "Recognition model not loaded!" << std::endl;
        return cv::Mat();
    }

    cv::Mat blob;
    if (expected_dim_ == 128) {
        // SFace preprocessing: normalize to [0, 1], no mean subtraction
        blob = cv::dnn::blobFromImage(aligned_face, 1.0 / 255.0,
                                       cv::Size(112, 112),
                                       cv::Scalar(),
                                       false, false);
    } else {
        // ArcFace preprocessing: (img - 127.5) / 127.5 → normalize to [-1, 1]
        blob = cv::dnn::blobFromImage(aligned_face, 1.0 / 127.5,
                                       cv::Size(112, 112),
                                       cv::Scalar(127.5, 127.5, 127.5),
                                       false, false);
    }

    arcface_net_.setInput(blob);
    cv::Mat embedding = arcface_net_.forward();

    // L2 normalize the embedding
    cv::normalize(embedding, embedding, 1.0, 0.0, cv::NORM_L2);
    return embedding.clone();
}

std::pair<bool, std::string> FaceRecognitionEngine::register_face(const cv::Mat& raw_image,
                                                                    const std::string& person_id) {
    cv::Mat image = raw_image.clone();
    apply_clahe(image);

    auto faces = detect_faces(image);
    if (faces.empty()) {
        return {false, "No face detected"};
    }
    int idx = select_largest_face(faces);
    cv::Mat aligned = align_face(image, faces[idx]);
    if (aligned.empty()) {
        return {false, "Face alignment failed"};
    }
    cv::Mat embedding = get_embedding(aligned);
    if (embedding.empty()) {
        return {false, "Failed to compute embedding (model not loaded?)"};
    }
    int dim = embedding.total();

    // Check for dimension mismatch with existing database
    if (!database_.empty()) {
        int existing_dim = database_.begin()->second.total();
        if (existing_dim != dim) {
            std::cout << "Embedding dim mismatch (db=" << existing_dim
                      << ", new=" << dim << "). Clearing database..." << std::endl;
            database_.clear();
        }
    }

    database_[person_id] = embedding;
    save_database();
    return {true, "Registered " + person_id + " successfully"};
}

std::tuple<std::string, double, std::string> FaceRecognitionEngine::recognize_face(const cv::Mat& raw_image,
                                                                                    double threshold) {
    cv::Mat image = raw_image.clone();
    apply_clahe(image);

    auto faces = detect_faces(image);
    if (faces.empty()) {
        return {"", 0.0, "No face detected"};
    }
    int idx = select_largest_face(faces);
    cv::Mat aligned = align_face(image, faces[idx]);
    if (aligned.empty()) {
        return {"", 0.0, "Face alignment failed"};
    }
    cv::Mat embedding = get_embedding(aligned);
    if (embedding.empty()) {
        return {"", 0.0, "Failed to compute embedding"};
    }

    std::string best_match;
    double best_score = -1;
    int query_dim = embedding.total();
    int compared = 0, skipped = 0;
    for (const auto& [person_id, stored_emb] : database_) {
        if (stored_emb.total() != (size_t)query_dim) {
            skipped++;
            continue;
        }
        compared++;
        // Cosine similarity = dot product of L2-normalized vectors
        double score = embedding.dot(stored_emb);
        if (score > best_score) {
            best_score = score;
            best_match = person_id;
        }
    }
    std::cout << "  Recognize: db=" << database_.size()
              << ", query_dim=" << query_dim
              << ", compared=" << compared
              << ", skipped=" << skipped
              << ", best=" << best_score
              << std::endl;

    if (best_score >= threshold) {
        return {best_match, best_score, "Match found"};
    }
    return {"", best_score, "No match found"};
}

void FaceRecognitionEngine::save_database() {
    std::ofstream f(db_path_);
    if (!f) return;
    f << "{";
    bool first = true;
    for (const auto& [person_id, emb] : database_) {
        if (!first) f << ", ";
        first = false;
        f << "\"" << person_id << "\": [";
        for (int j = 0; j < emb.total(); j++) {
            if (j > 0) f << ", ";
            f << emb.at<float>(j);
        }
        f << "]";
    }
    f << "}" << std::endl;
}

void FaceRecognitionEngine::load_database() {
    std::ifstream f(db_path_);
    if (!f) return;
    std::stringstream buffer;
    buffer << f.rdbuf();
    std::string content = buffer.str();

    // Check dimension consistency
    int ref_dim = -1;
    bool dim_ok = true;
    size_t pos = 0;
    while (true) {
        size_t key_start = content.find('"', pos);
        if (key_start == std::string::npos) break;
        size_t key_end = content.find('"', key_start + 1);
        if (key_end == std::string::npos) break;
        size_t arr_start = content.find('[', key_end);
        if (arr_start == std::string::npos) break;
        size_t arr_end = content.find(']', arr_start);
        if (arr_end == std::string::npos) break;

        std::string nums = content.substr(arr_start + 1, arr_end - arr_start - 1);
        int count = 1;
        for (char c : nums) if (c == ',') count++;

        if (ref_dim < 0) ref_dim = count;
        else if (count != ref_dim) { dim_ok = false; break; }

        pos = arr_end + 1;
    }

    // Clear database if dimension doesn't match the loaded model
    if (!dim_ok || (ref_dim > 0 && ref_dim != expected_dim_)) {
        std::cout << "Database dimension mismatch (db=" << ref_dim
                  << ", model=" << expected_dim_ << "). Clearing..." << std::endl;
        database_.clear();
        save_database();
        return;
    }

    // Load entries
    pos = 0;
    while (true) {
        size_t key_start = content.find('"', pos);
        if (key_start == std::string::npos) break;
        size_t key_end = content.find('"', key_start + 1);
        if (key_end == std::string::npos) break;
        std::string person_id = content.substr(key_start + 1, key_end - key_start - 1);

        size_t arr_start = content.find('[', key_end);
        if (arr_start == std::string::npos) break;
        size_t arr_end = content.find(']', arr_start);
        if (arr_end == std::string::npos) break;

        std::string nums = content.substr(arr_start + 1, arr_end - arr_start - 1);
        std::vector<float> emb;
        std::stringstream ss(nums);
        std::string token;
        while (std::getline(ss, token, ',')) {
            try { emb.push_back(std::stof(token)); } catch (...) {}
        }

        cv::Mat mat(1, (int)emb.size(), CV_32F);
        for (int i = 0; i < (int)emb.size(); i++) mat.at<float>(i) = emb[i];
        database_[person_id] = mat;

        pos = arr_end + 1;
    }
    std::cout << "Loaded " << database_.size() << " registered faces (dim=" << ref_dim << ")" << std::endl;
}
