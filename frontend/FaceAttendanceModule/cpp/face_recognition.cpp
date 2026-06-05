#include "face_recognition.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>

// 112x112 ArcFace reference landmarks
static const float ARC_REF_LM[5][2] = {
    {38.2946f, 51.6963f},
    {73.5318f, 51.5014f},
    {56.0252f, 71.7366f},
    {41.5493f, 92.3655f},
    {70.7299f, 92.2041f},
};

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
            if (!suppressed[jdx] && iou(rects[idx], rects[jdx]) > threshold)
                suppressed[jdx] = true;
        }
    }
    return keep;
}

// ─── Constructor ──────────────────────────────────────────────────────────────

FaceRecognitionEngine::FaceRecognitionEngine(const std::string& yunet_path,
                                             const std::string& recog_path,
                                             const std::string& db_path)
    : yunet_path_(yunet_path), recog_path_(recog_path),
      db_path_(db_path), expected_dim_(0)
{
    if (load_models()) {
        load_database();
        std::cout << "[FaceEngine] Ready! dim=" << expected_dim_ << std::endl;
    } else {
        std::cerr << "[FaceEngine] Failed to initialize!" << std::endl;
    }
}

FaceRecognitionEngine::~FaceRecognitionEngine() {}

// ─── Model Loading ────────────────────────────────────────────────────────────

bool FaceRecognitionEngine::load_models() {
    yunet_ = cv::dnn::readNetFromONNX(yunet_path_);
    if (yunet_.empty()) {
        std::cerr << "[FaceEngine] Failed to load YuNet: " << yunet_path_ << std::endl;
        return false;
    }

    recog_net_ = cv::dnn::readNetFromONNX(recog_path_);
    if (recog_net_.empty()) {
        std::cerr << "[FaceEngine] Failed to load recognition model: " << recog_path_ << std::endl;
        return false;
    }
    recog_net_.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
    recog_net_.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);

    // Dummy forward to get output dim
    cv::Mat dummy = cv::Mat::zeros(1, 3 * 112 * 112, CV_32F);
    dummy = dummy.reshape(1, {1, 3, 112, 112});
    recog_net_.setInput(dummy);
    cv::Mat out = recog_net_.forward();
    expected_dim_ = out.total();
    std::cout << "[FaceEngine] Model output dim: " << expected_dim_ << std::endl;
    return true;
}

// ─── CLAHE ───────────────────────────────────────────────────────────────────

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

std::vector<cv::Mat> FaceRecognitionEngine::detect_faces(const cv::Mat& image) {
    int h = image.rows, w = image.cols;
    float sx = (float)w / 640.0f;
    float sy = (float)h / 640.0f;

    cv::Mat blob = cv::dnn::blobFromImage(image, 1.0 / 127.5, cv::Size(640, 640),
                                          cv::Scalar(127.5, 127.5, 127.5), false, false);
    yunet_.setInput(blob);
    cv::Mat output = yunet_.forward();

    int num = output.size[1];
    const float* data = output.ptr<float>(0);

    std::vector<cv::Rect> rects;
    std::vector<float>    scores;
    std::vector<int>      valid_idx;

    for (int i = 0; i < num; i++) {
        const float* row = data + i * 15;
        if (row[4] < 0.5f) continue;
        float cx = row[0] * sx, cy = row[1] * sy;
        float bw = row[2] * sx, bh = row[3] * sy;
        int x1 = std::max(0, (int)(cx - bw / 2.0f));
        int y1 = std::max(0, (int)(cy - bh / 2.0f));
        int x2 = std::min(w - 1, (int)(cx + bw / 2.0f));
        int y2 = std::min(h - 1, (int)(cy + bh / 2.0f));
        rects.push_back(cv::Rect(x1, y1, x2 - x1, y2 - y1));
        scores.push_back(row[4]);
        valid_idx.push_back(i);
    }

    std::vector<cv::Mat> faces;
    for (int k : nms(rects, scores, 0.3f)) {
        int idx = valid_idx[k];
        const float* row = data + idx * 15;
        float cx = row[0] * sx, cy = row[1] * sy;
        float bw = row[2] * sx, bh = row[3] * sy;
        cv::Mat face(1, 15, CV_32F);
        face.at<float>(0, 0) = cx - bw / 2.0f;
        face.at<float>(0, 1) = cy - bh / 2.0f;
        face.at<float>(0, 2) = bw;
        face.at<float>(0, 3) = bh;
        face.at<float>(0, 4) = row[4];
        // odd cols = x, even cols = y
        for (int j = 5; j < 15; j++)
            face.at<float>(0, j) = (j % 2 == 1) ? row[j] * sx : row[j] * sy;
        faces.push_back(face);
    }
    return faces;
}

// ─── Face Alignment ───────────────────────────────────────────────────────────

cv::Mat FaceRecognitionEngine::align_face(const cv::Mat& image, const cv::Mat& face) {
    std::vector<cv::Point2f> src(5), dst(5);
    for (int i = 0; i < 5; i++) {
        src[i] = cv::Point2f(face.at<float>(0, 5 + i*2), face.at<float>(0, 6 + i*2));
        dst[i] = cv::Point2f(ARC_REF_LM[i][0], ARC_REF_LM[i][1]);
    }
    cv::Mat tform = cv::estimateAffinePartial2D(src, dst, cv::noArray(), cv::LMEDS);
    if (tform.empty())
        tform = cv::estimateAffinePartial2D(src, dst, cv::noArray(), cv::RANSAC);
    if (tform.empty())
        tform = cv::getAffineTransform(src.data(), dst.data());
    cv::Mat aligned;
    cv::warpAffine(image, aligned, tform, cv::Size(112, 112),
                   cv::INTER_LINEAR, cv::BORDER_REFLECT);
    return aligned;
}

static int select_largest_face(const std::vector<cv::Mat>& faces) {
    if (faces.empty()) return -1;
    int best = 0; float best_area = 0;
    for (int i = 0; i < (int)faces.size(); i++) {
        float area = faces[i].at<float>(0,2) * faces[i].at<float>(0,3);
        if (area > best_area) { best_area = area; best = i; }
    }
    return best;
}

// ─── Embedding ────────────────────────────────────────────────────────────────

cv::Mat FaceRecognitionEngine::get_embedding(const cv::Mat& aligned_face) {
    if (recog_net_.empty()) return cv::Mat();
    cv::Mat blob = cv::dnn::blobFromImage(aligned_face, 1.0 / 127.5,
                                          cv::Size(112, 112),
                                          cv::Scalar(127.5, 127.5, 127.5),
                                          false, false);
    recog_net_.setInput(blob);
    cv::Mat embedding = recog_net_.forward();
    cv::normalize(embedding, embedding, 1.0, 0.0, cv::NORM_L2);
    return embedding.clone();
}

// ─── Register ─────────────────────────────────────────────────────────────────
// Stores up to MAX_EMBEDDINGS_PER_PERSON embeddings per person.
// Recognition takes the MAX score across all embeddings.

std::pair<bool, std::string> FaceRecognitionEngine::register_face(
    const cv::Mat& raw_image, const std::string& person_id)
{
    cv::Mat image = raw_image.clone();
    apply_clahe(image);

    auto faces = detect_faces(image);
    if (faces.empty()) return {false, "No face detected"};

    int idx = select_largest_face(faces);
    cv::Mat aligned = align_face(image, faces[idx]);
    if (aligned.empty()) return {false, "Face alignment failed"};

    cv::Mat embedding = get_embedding(aligned);
    if (embedding.empty()) return {false, "Failed to compute embedding"};

    auto& emb_list = database_[person_id];

    // If at max capacity, remove the oldest embedding
    if ((int)emb_list.size() >= MAX_EMBEDDINGS_PER_PERSON) {
        emb_list.erase(emb_list.begin());
    }
    emb_list.push_back(embedding);

    save_database();
    std::cout << "[FaceEngine] Registered: " << person_id
              << " embeddings=" << emb_list.size()
              << " dim=" << embedding.total() << std::endl;
    return {true, "Registered " + person_id + " ("
            + std::to_string(emb_list.size()) + "/" 
            + std::to_string(MAX_EMBEDDINGS_PER_PERSON) + " samples)"};
}

// ─── Recognize ────────────────────────────────────────────────────────────────
// Takes MAX score across all stored embeddings for each person.

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
    int query_dim = embedding.total();

    for (const auto& [pid, emb_list] : database_) {
        // Take max score across all embeddings for this person
        for (const auto& stored : emb_list) {
            if ((int)stored.total() != query_dim) continue;
            double score = embedding.dot(stored);
            if (score > best_score) {
                best_score = score;
                best_match = pid;
            }
        }
    }

    std::cout << "[FaceEngine] Recognize: best=" << best_score
              << " person=" << best_match
              << " threshold=" << threshold << std::endl;

    if (best_score >= threshold)
        return {best_match, best_score, "Match found"};
    return {"", best_score, "No match (best=" + std::to_string(best_score) + ")"};
}

// ─── Persistence ──────────────────────────────────────────────────────────────
// Format: { "person_id": [[emb1...], [emb2...], ...], ... }

void FaceRecognitionEngine::save_database() {
    std::ofstream f(db_path_);
    if (!f) return;
    f << "{";
    bool first_person = true;
    for (const auto& [pid, emb_list] : database_) {
        if (!first_person) f << ",";
        first_person = false;
        f << "\"" << pid << "\":[";
        for (int e = 0; e < (int)emb_list.size(); e++) {
            if (e > 0) f << ",";
            f << "[";
            for (int j = 0; j < (int)emb_list[e].total(); j++) {
                if (j > 0) f << ",";
                f << emb_list[e].at<float>(j);
            }
            f << "]";
        }
        f << "]";
    }
    f << "}" << std::endl;
    std::cout << "[FaceEngine] Saved " << database_.size() << " people" << std::endl;
}

void FaceRecognitionEngine::load_database() {
    std::ifstream f(db_path_);
    if (!f) return;
    std::stringstream buf; buf << f.rdbuf();
    std::string content = buf.str();
    if (content.empty() || content == "{}" || content == "{}\n") return;

    // Parse: "person_id":[[...],[...],...]
    size_t pos = 0;
    int loaded = 0;
    while (true) {
        // Find person key
        size_t ks = content.find('"', pos);
        if (ks == std::string::npos) break;
        size_t ke = content.find('"', ks + 1);
        if (ke == std::string::npos) break;
        std::string pid = content.substr(ks + 1, ke - ks - 1);

        // Find outer array start
        size_t outer_start = content.find('[', ke);
        if (outer_start == std::string::npos) break;

        // Check if it's [[...]] (new format) or [...] (old single embedding format)
        size_t next_char_pos = outer_start + 1;
        while (next_char_pos < content.size() && content[next_char_pos] == ' ') next_char_pos++;

        std::vector<cv::Mat> emb_list;

        if (next_char_pos < content.size() && content[next_char_pos] == '[') {
            // New format: [[emb1], [emb2], ...]
            size_t scan = outer_start;
            while (true) {
                size_t inner_start = content.find('[', scan + 1);
                if (inner_start == std::string::npos) break;
                // Make sure we haven't gone past outer array
                size_t outer_end = content.find(']', outer_start + 1);
                // Find matching inner end
                size_t inner_end = content.find(']', inner_start + 1);
                if (inner_end == std::string::npos) break;
                if (inner_start > outer_end) break;

                std::string nums = content.substr(inner_start + 1, inner_end - inner_start - 1);
                std::vector<float> emb;
                std::stringstream ss(nums); std::string token;
                while (std::getline(ss, token, ','))
                    try { emb.push_back(std::stof(token)); } catch (...) {}

                if (!emb.empty() && (expected_dim_ == 0 || (int)emb.size() == expected_dim_)) {
                    cv::Mat mat(1, (int)emb.size(), CV_32F);
                    for (int i = 0; i < (int)emb.size(); i++) mat.at<float>(i) = emb[i];
                    emb_list.push_back(mat);
                }
                scan = inner_end;
                // Check if next non-space char after inner_end is ] (end of outer)
                size_t nxt = inner_end + 1;
                while (nxt < content.size() && content[nxt] == ' ') nxt++;
                if (nxt < content.size() && content[nxt] == ']') {
                    pos = nxt + 1;
                    break;
                }
            }
        } else {
            // Old format: [emb...] — single embedding, wrap in list
            size_t outer_end = content.find(']', outer_start + 1);
            if (outer_end == std::string::npos) break;
            std::string nums = content.substr(outer_start + 1, outer_end - outer_start - 1);
            std::vector<float> emb;
            std::stringstream ss(nums); std::string token;
            while (std::getline(ss, token, ','))
                try { emb.push_back(std::stof(token)); } catch (...) {}
            if (!emb.empty() && (expected_dim_ == 0 || (int)emb.size() == expected_dim_)) {
                cv::Mat mat(1, (int)emb.size(), CV_32F);
                for (int i = 0; i < (int)emb.size(); i++) mat.at<float>(i) = emb[i];
                emb_list.push_back(mat);
            }
            pos = outer_end + 1;
        }

        if (!emb_list.empty()) {
            database_[pid] = emb_list;
            loaded++;
        }
    }
    std::cout << "[FaceEngine] Loaded " << loaded << " people from DB" << std::endl;
}
