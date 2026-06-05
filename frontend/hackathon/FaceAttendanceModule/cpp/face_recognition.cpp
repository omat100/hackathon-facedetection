#include "face_recognition.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>

// 112x112 reference landmarks for similarity alignment
static const float ARC_REF_LM[5][2] = {
    {38.2946f, 51.6963f},
    {73.5318f, 51.5014f},
    {56.0252f, 71.7366f},
    {41.5493f, 92.3655f},
    {70.7299f, 92.2041f},
};

static float iou(const cv::Rect& a, const cv::Rect& b) {
    float inter = (a & b).area();
    float uni = a.area() + b.area() - inter;
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

FaceRecognitionEngine::FaceRecognitionEngine(const std::string& yunet_path,
                                             const std::string& sface_path,
                                             const std::string& db_path)
    : expected_dim_(0),
      yunet_path_(yunet_path),
      sface_path_(sface_path),
      db_path_(db_path)
{
    if (load_models()) {
        load_database();
        std::cout << "Face Recognition Engine ready! (dim=" << expected_dim_ << ")" << std::endl;
    } else {
        std::cerr << "Face Recognition Engine failed to initialize!" << std::endl;
    }
}

FaceRecognitionEngine::~FaceRecognitionEngine() {}

// ─── Model Loading ────────────────────────────────────────────────────────────

bool FaceRecognitionEngine::load_models() {
    yunet_ = cv::dnn::readNetFromONNX(yunet_path_);
    if (yunet_.empty()) {
        std::cerr << "Failed to load YuNet model: " << yunet_path_ << std::endl;
        return false;
    }

    arcface_net_ = cv::dnn::readNetFromONNX(sface_path_);
    if (arcface_net_.empty()) {
        std::cerr << "Failed to load SFace model: " << sface_path_ << std::endl;
        return false;
    }

    arcface_net_.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
    arcface_net_.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
    // CHANGED: buffalo_sc w600k_mbf outputs 512-dim embeddings (vs SFace 128-dim)
    expected_dim_ = 512;
    std::cout << "Loaded models (dim=" << expected_dim_ << ")" << std::endl;
    return true;
}

// ─── Image Preprocessing ─────────────────────────────────────────────────────

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

// ─── Face Detection ───────────────────────────────────────────────────────────
// FIX: YuNet 2023mar outputs [cx, cy, w, h] (center format), not [x, y, w, h].
// Landmarks are also in absolute coords after scaling.

std::vector<cv::Mat> FaceRecognitionEngine::detect_faces(const cv::Mat& image) {
    int h = image.rows, w = image.cols;

    cv::Mat blob = cv::dnn::blobFromImage(image, 1.0 / 127.5, cv::Size(640, 640),
                                          cv::Scalar(127.5, 127.5, 127.5), false, false);
    yunet_.setInput(blob);
    cv::Mat output = yunet_.forward();

    int num_detections = output.size[1];
    const float* data = output.ptr<float>(0);

    float scale_x = (float)w / 640.0f;
    float scale_y = (float)h / 640.0f;

    std::vector<cv::Rect> rects;
    std::vector<float> scores;
    std::vector<int> valid_indices;

    for (int i = 0; i < num_detections; i++) {
        const float* row = data + i * 15;
        float conf = row[4];
        if (conf < 0.5f) continue;

        // FIX: row[0],row[1] = cx,cy; row[2],row[3] = w,h (center format)
        float cx = row[0] * scale_x;
        float cy = row[1] * scale_y;
        float bw = row[2] * scale_x;
        float bh = row[3] * scale_y;
        int x1 = (int)(cx - bw / 2.0f);
        int y1 = (int)(cy - bh / 2.0f);

        cv::Rect r(x1, y1, (int)bw, (int)bh);
        rects.push_back(r);
        scores.push_back(conf);
        valid_indices.push_back(i);
    }

    std::vector<cv::Mat> faces;
    auto keep = nms(rects, scores, 0.3f);
    for (int k : keep) {
        int idx = valid_indices[k];
        const float* row = data + idx * 15;

        float cx = row[0] * scale_x;
        float cy = row[1] * scale_y;
        float bw = row[2] * scale_x;
        float bh = row[3] * scale_y;

        cv::Mat face(1, 15, CV_32F);
        // Store as top-left x,y for downstream alignment
        face.at<float>(0, 0) = cx - bw / 2.0f;
        face.at<float>(0, 1) = cy - bh / 2.0f;
        face.at<float>(0, 2) = bw;
        face.at<float>(0, 3) = bh;
        face.at<float>(0, 4) = row[4];
        // Landmarks: cols 5-14, alternating x,y pairs, already absolute after scaling
        for (int j = 5; j < 15; j++)
            face.at<float>(0, j) = (j % 2 == 1) ? row[j] * scale_x : row[j] * scale_y;
        faces.push_back(face);
    }
    return faces;
}

// ─── Face Alignment ───────────────────────────────────────────────────────────

cv::Mat FaceRecognitionEngine::align_face(const cv::Mat& image, const cv::Mat& face) {
    std::vector<cv::Point2f> src(5), dst(5);
    src[0] = cv::Point2f(face.at<float>(0, 5),  face.at<float>(0, 6));
    src[1] = cv::Point2f(face.at<float>(0, 7),  face.at<float>(0, 8));
    src[2] = cv::Point2f(face.at<float>(0, 9),  face.at<float>(0, 10));
    src[3] = cv::Point2f(face.at<float>(0, 11), face.at<float>(0, 12));
    src[4] = cv::Point2f(face.at<float>(0, 13), face.at<float>(0, 14));

    for (int i = 0; i < 5; i++)
        dst[i] = cv::Point2f(ARC_REF_LM[i][0], ARC_REF_LM[i][1]);

    cv::Mat tform = cv::estimateAffinePartial2D(src, dst);
    if (tform.empty())
        tform = cv::getAffineTransform(src.data(), dst.data());

    cv::Mat aligned;
    cv::warpAffine(image, aligned, tform, cv::Size(112, 112));
    return aligned;
}

// ─── Select Largest Face ──────────────────────────────────────────────────────

static int select_largest_face(const std::vector<cv::Mat>& faces) {
    if (faces.empty()) return -1;
    int best_idx = 0;
    float best_area = 0;
    for (int i = 0; i < (int)faces.size(); i++) {
        float fw = faces[i].at<float>(0, 2);
        float fh = faces[i].at<float>(0, 3);
        float area = fw * fh;
        if (area > best_area) {
            best_area = area;
            best_idx = i;
        }
    }
    return best_idx;
}

// ─── Embedding ────────────────────────────────────────────────────────────────

cv::Mat FaceRecognitionEngine::get_embedding(const cv::Mat& aligned_face) {
    if (arcface_net_.empty()) {
        std::cerr << "Recognition model not loaded!" << std::endl;
        return cv::Mat();
    }

    // buffalo_sc w600k_mbf uses ArcFace preprocessing: (img - 127.5) / 127.5
    cv::Mat blob = cv::dnn::blobFromImage(aligned_face, 1.0 / 127.5,
                                          cv::Size(112, 112),
                                          cv::Scalar(127.5, 127.5, 127.5), false, false);

    arcface_net_.setInput(blob);
    cv::Mat embedding = arcface_net_.forward();
    cv::normalize(embedding, embedding, 1.0, 0.0, cv::NORM_L2);
    return embedding.clone();
}

// ─── Register Face ────────────────────────────────────────────────────────────

std::pair<bool, std::string> FaceRecognitionEngine::register_face(const cv::Mat& raw_image,
                                                                    const std::string& person_id) {
    cv::Mat image = raw_image.clone();
    apply_clahe(image);

    auto faces = detect_faces(image);
    if (faces.empty()) return {false, "No face detected"};

    int idx = select_largest_face(faces);
    cv::Mat aligned = align_face(image, faces[idx]);
    if (aligned.empty()) return {false, "Face alignment failed"};

    cv::Mat embedding = get_embedding(aligned);
    if (embedding.empty()) return {false, "Failed to compute embedding"};

    int dim = static_cast<int>(embedding.total());
    if (!database_.empty()) {
        int existing_dim = database_.begin()->second.total();
        if (existing_dim != dim) {
            std::cout << "Embedding dim mismatch — clearing DB..." << std::endl;
            database_.clear();
        }
    }

    database_[person_id] = embedding;
    save_database();
    return {true, "Registered " + person_id + " successfully"};
}

// ─── Recognize Face ───────────────────────────────────────────────────────────

std::tuple<std::string, double, std::string>
FaceRecognitionEngine::recognize_face(const cv::Mat& raw_image, double threshold) {
    cv::Mat image = raw_image.clone();
    apply_clahe(image);

    auto faces = detect_faces(image);
    if (faces.empty()) return {"", 0.0, "No face detected"};

    int idx = select_largest_face(faces);
    cv::Mat aligned = align_face(image, faces[idx]);
    if (aligned.empty()) return {"", 0.0, "Face alignment failed"};

    cv::Mat embedding = get_embedding(aligned);
    if (embedding.empty()) return {"", 0.0, "Failed to compute embedding"};

    std::string best_match;
    double best_score = -1;
    int query_dim = static_cast<int>(embedding.total());
    int compared = 0, skipped = 0;

    for (const auto& [pid, stored_emb] : database_) {
        if ((int)stored_emb.total() != query_dim) { skipped++; continue; }
        compared++;
        double score = embedding.dot(stored_emb);
        if (score > best_score) {
            best_score = score;
            best_match = pid;
        }
    }

    std::cout << "Recognize: db=" << database_.size()
              << " query_dim=" << query_dim
              << " compared=" << compared
              << " skipped=" << skipped
              << " best=" << best_score << std::endl;

    if (best_score >= threshold)
        return {best_match, best_score, "Match found"};
    return {"", best_score, "No match found"};
}

// ─── Persistence ──────────────────────────────────────────────────────────────

void FaceRecognitionEngine::save_database() {
    std::ofstream f(db_path_);
    if (!f) return;
    f << "{";
    bool first = true;
    for (const auto& [pid, emb] : database_) {
        if (!first) f << ", ";
        first = false;
        f << "\"" << pid << "\": [";
        for (int j = 0; j < (int)emb.total(); j++) {
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

    int ref_dim = -1;
    bool dim_ok = true;
    size_t pos = 0;

    // First pass: validate dimensions
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

    if (!dim_ok || (ref_dim > 0 && ref_dim != expected_dim_)) {
        std::cout << "DB dim mismatch (db=" << ref_dim
                  << " model=" << expected_dim_ << "). Clearing." << std::endl;
        database_.clear();
        save_database();
        return;
    }

    // Second pass: load data
    pos = 0;
    while (true) {
        size_t key_start = content.find('"', pos);
        if (key_start == std::string::npos) break;
        size_t key_end = content.find('"', key_start + 1);
        if (key_end == std::string::npos) break;
        std::string pid = content.substr(key_start + 1, key_end - key_start - 1);

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
        database_[pid] = mat;
        pos = arr_end + 1;
    }
    std::cout << "Loaded " << database_.size()
              << " registered faces (dim=" << ref_dim << ")" << std::endl;
}
