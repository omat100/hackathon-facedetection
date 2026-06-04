#pragma once
#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
#include <string>
#include <vector>
#include <unordered_map>

class FaceRecognitionEngine {
public:
    FaceRecognitionEngine(const std::string& model_dir, const std::string& db_path = "face_database.json");
    ~FaceRecognitionEngine();

    std::pair<bool, std::string> register_face(const cv::Mat& image, const std::string& person_id);
    std::tuple<std::string, double, std::string> recognize_face(const cv::Mat& image, double threshold = 0.3);

    void set_db_path(const std::string& path) { db_path_ = path; }
    int database_size() const { return database_.size(); }

private:
    cv::Ptr<cv::FaceDetectorYN> detector_;
    cv::dnn::Net arcface_net_;
    std::unordered_map<std::string, cv::Mat> database_;
    std::string db_path_;
    std::string model_dir_;
    int expected_dim_ = 0;

    void ensure_models();
    void apply_clahe(cv::Mat& frame);
    cv::Mat align_face(const cv::Mat& image, const cv::Mat& face);
    std::vector<cv::Mat> detect_faces(const cv::Mat& image);
    cv::Mat get_embedding(const cv::Mat& aligned_face);
    void save_database();
    void load_database();
};
