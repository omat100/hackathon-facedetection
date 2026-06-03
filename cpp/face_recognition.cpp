#include "face_recognition.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstdlib>
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

static const std::string YUNET_URL =
    "https://github.com/opencv/opencv_zoo/raw/main/models/face_detection_yunet/"
    "face_detection_yunet_2023mar.onnx";
static const std::string SFACE_URL =
    "https://github.com/opencv/opencv_zoo/raw/main/models/face_recognition_sface/"
    "face_recognition_sface_2021dec.onnx";

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

void FaceRecognitionEngine::ensure_models() {
    std::string detector_path = download_file(YUNET_URL, "face_detection_yunet_2023mar.onnx");
    std::string recognizer_path = download_file(SFACE_URL, "face_recognition_sface_2021dec.onnx");

    detector_ = cv::FaceDetectorYN::create(detector_path, "", cv::Size(320, 320), 0.6f, 0.3f, 5000);
    recognizer_ = cv::FaceRecognizerSF::create(recognizer_path, "");
}

FaceRecognitionEngine::FaceRecognitionEngine() {
    ensure_models();
    load_database();
    std::cout << "Face Recognition Engine ready!" << std::endl;
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

cv::Mat FaceRecognitionEngine::align_face(const cv::Mat& image, const std::vector<cv::Mat>& faces, int idx) {
    if (faces.empty()) return cv::Mat();
    cv::Mat aligned;
    recognizer_->alignCrop(image, faces[idx], aligned);
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
    cv::Mat feat;
    recognizer_->feature(aligned_face, feat);
    return feat.clone();
}

std::pair<bool, std::string> FaceRecognitionEngine::register_face(const cv::Mat& image, const std::string& person_id) {
    auto faces = detect_faces(image);
    if (faces.empty()) {
        return {false, "No face detected"};
    }
    int idx = select_largest_face(faces);
    cv::Mat aligned = align_face(image, faces, idx);
    if (aligned.empty()) {
        return {false, "Face alignment failed"};
    }
    cv::Mat embedding = get_embedding(aligned);
    int dim = embedding.cols;

    // Check for dimension mismatch with existing database
    if (!database_.empty()) {
        int existing_dim = database_.begin()->second.cols;
        if (existing_dim != dim) {
            std::cout << "Embedding dimension mismatch (db=" << existing_dim
                      << ", new=" << dim << "). Clearing database..." << std::endl;
            database_.clear();
        }
    }

    database_[person_id] = embedding.clone();
    save_database();
    return {true, "Registered " + person_id + " successfully"};
}

std::tuple<std::string, double, std::string> FaceRecognitionEngine::recognize_face(const cv::Mat& image, double threshold) {
    auto faces = detect_faces(image);
    if (faces.empty()) {
        return {"", 0.0, "No face detected"};
    }
    int idx = select_largest_face(faces);
    cv::Mat aligned = align_face(image, faces, idx);
    if (aligned.empty()) {
        return {"", 0.0, "Face alignment failed"};
    }
    cv::Mat embedding = get_embedding(aligned);

    std::string best_match;
    double best_score = -1;
    int query_dim = embedding.cols;
    for (const auto& [person_id, stored_emb] : database_) {
        if (stored_emb.cols != query_dim) {
            std::cout << "  Skipping " << person_id << " (dim mismatch: "
                      << stored_emb.cols << " vs " << query_dim << ")" << std::endl;
            continue;
        }
        double score = recognizer_->match(embedding, stored_emb, cv::FaceRecognizerSF::DisType::FR_COSINE);
        if (score > best_score) {
            best_score = score;
            best_match = person_id;
        }
    }

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
        for (int j = 0; j < emb.cols; j++) {
            if (j > 0) f << ", ";
            f << emb.at<float>(0, j);
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

    // First pass: check all dimensions
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

    if (!dim_ok) {
        std::cout << "Database has inconsistent embedding dimensions. Clearing..." << std::endl;
        database_.clear();
        save_database();
        return;
    }

    // Second pass: load if dimensions match expected
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

        cv::Mat mat(1, emb.size(), CV_32F);
        for (int i = 0; i < (int)emb.size(); i++) mat.at<float>(0, i) = emb[i];
        database_[person_id] = mat;

        pos = arr_end + 1;
    }
    std::cout << "Loaded " << database_.size() << " registered faces (dim=" << ref_dim << ")" << std::endl;
}
