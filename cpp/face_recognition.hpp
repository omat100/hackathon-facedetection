#pragma once
#include <opencv2/objdetect.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include "storage.hpp"

class FaceRecognitionEngine {
public:
    FaceRecognitionEngine();
    ~FaceRecognitionEngine();

    std::pair<bool, std::string> register_face(const cv::Mat& image, const std::string& person_id);
    std::tuple<std::string, double, std::string> recognize_face(const cv::Mat& image, double threshold = 0.4);

private:
    cv::Ptr<cv::FaceDetectorYN> detector_;
    cv::Ptr<cv::FaceRecognizerSF> recognizer_;
    std::unordered_map<std::string, cv::Mat> database_;
    std::string db_path_ = "face_database.json";

    void ensure_models();
    cv::Mat get_embedding(const cv::Mat& aligned_face);
    std::vector<cv::Mat> detect_faces(const cv::Mat& image);
    cv::Mat align_face(const cv::Mat& image, const std::vector<cv::Mat>& faces, int idx);
    void save_database();
    void load_database();
    static std::string download_file(const std::string& url, const std::string& dest);
};
