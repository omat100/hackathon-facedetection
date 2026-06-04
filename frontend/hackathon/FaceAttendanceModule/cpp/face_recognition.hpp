#pragma once
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/calib3d.hpp>
#include <opencv2/dnn.hpp>
#include <string>
#include <vector>
#include <unordered_map>

class FaceRecognitionEngine {
public:
    // Accepts explicit paths to ONNX models and optional DB path
    FaceRecognitionEngine(const std::string& yunet_path,
                          const std::string& sface_path,
                          const std::string& db_path = "face_database.json");
    ~FaceRecognitionEngine();

    std::pair<bool, std::string> register_face(const cv::Mat& image, const std::string& person_id);
    std::tuple<std::string, double, std::string> recognize_face(const cv::Mat& image, double threshold = 0.3);

private:
    cv::dnn::Net yunet_;
    cv::dnn::Net arcface_net_;
    std::unordered_map<std::string, cv::Mat> database_;
    std::string db_path_;
    std::string yunet_path_;
    std::string sface_path_;
    int expected_dim_;

    bool load_models();
    void apply_clahe(cv::Mat& frame);
    cv::Mat align_face(const cv::Mat& image, const cv::Mat& face);
    std::vector<cv::Mat> detect_faces(const cv::Mat& image);
    cv::Mat get_embedding(const cv::Mat& aligned_face);
    void save_database();
    void load_database();
};
