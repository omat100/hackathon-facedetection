#include <jni.h>
#include <android/log.h>
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>
#include <sstream>

#include "face_recognition.hpp"
#include "liveness_detector.hpp"
#include "storage.hpp"

#define TAG "FaceAttendanceJNI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

static FaceRecognitionEngine* g_face_engine = nullptr;
static LivenessDetector* g_liveness = nullptr;
static AttendanceStorage* g_storage = nullptr;

static const char BASE64_CHARS[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int base64_decode(const std::string& in, std::vector<uint8_t>& out) {
    if (in.empty()) return -1;
    std::string data = in;
    size_t pos = data.find(',');
    if (pos != std::string::npos) data = data.substr(pos + 1);
    size_t len = data.size();
    if (len == 0) return -1;
    size_t pad = 0;
    if (data[len - 1] == '=') pad++;
    if (len > 1 && data[len - 2] == '=') pad++;
    size_t decoded_len = (len * 3) / 4 - pad;
    out.resize(decoded_len);
    int table[256];
    for (int i = 0; i < 256; i++) table[i] = -1;
    for (int i = 0; i < 64; i++) table[(int)BASE64_CHARS[i]] = i;
    size_t out_idx = 0;
    for (size_t i = 0; i < len; i += 4) {
        int b0 = table[(unsigned char)data[i]];
        int b1 = table[(unsigned char)data[i + 1]];
        int b2 = table[(unsigned char)data[i + 2]];
        int b3 = table[(unsigned char)data[i + 3]];
        if (b0 < 0 || b1 < 0) return -1;
        out[out_idx++] = (unsigned char)((b0 << 2) | (b1 >> 4));
        if (b2 >= 0) {
            out[out_idx++] = (unsigned char)(((b1 & 0x0F) << 4) | (b2 >> 2));
            if (b3 >= 0) {
                out[out_idx++] = (unsigned char)(((b2 & 0x03) << 6) | b3);
            }
        }
    }
    return 0;
}

static cv::Mat base64_to_mat(const std::string& base64) {
    std::vector<uint8_t> bytes;
    if (base64_decode(base64, bytes) != 0) return cv::Mat();
    return cv::imdecode(bytes, cv::IMREAD_COLOR);
}

static std::string make_json_result(bool liveness_verified, bool liveness_failed,
                                     const std::string& liveness_msg,
                                     const std::string& person_id,
                                     double confidence, bool recognized) {
    std::ostringstream json;
    json << "{"
         << "\"liveness_verified\":" << (liveness_verified ? "true" : "false") << ","
         << "\"liveness_failed\":" << (liveness_failed ? "true" : "false") << ","
         << "\"liveness_message\":\"" << liveness_msg << "\","
         << "\"person_id\":\"" << person_id << "\","
         << "\"confidence\":" << confidence << ","
         << "\"recognized\":" << (recognized ? "true" : "false")
         << "}";
    return json.str();
}

extern "C" {

JNIEXPORT jboolean JNICALL
Java_com_anonymous_hackathon_FaceAttendanceModule_nativeInit(
    JNIEnv* env, jobject thiz, jstring model_dir, jstring db_path) {
    try {
        if (g_face_engine) {
            delete g_face_engine;
            g_face_engine = nullptr;
        }
        if (g_liveness) {
            delete g_liveness;
            g_liveness = nullptr;
        }
        if (g_storage) {
            delete g_storage;
            g_storage = nullptr;
        }

        const char* model_dir_c = env->GetStringUTFChars(model_dir, nullptr);
        const char* db_path_c = env->GetStringUTFChars(db_path, nullptr);

        std::string md(model_dir_c);
        std::string dp(db_path_c);

        env->ReleaseStringUTFChars(model_dir, model_dir_c);
        env->ReleaseStringUTFChars(db_path, db_path_c);

        g_face_engine = new FaceRecognitionEngine(md, dp);
        g_liveness = new LivenessDetector(md);
        g_storage = new AttendanceStorage(dp);

        LOGI("JNI nativeInit complete");
        return JNI_TRUE;
    } catch (const std::exception& e) {
        LOGE("nativeInit failed: %s", e.what());
        return JNI_FALSE;
    }
}

JNIEXPORT jstring JNICALL
Java_com_anonymous_hackathon_FaceAttendanceModule_nativeProcessBase64(
    JNIEnv* env, jobject thiz, jstring base64_jstr) {
    try {
        if (!g_face_engine || !g_liveness) {
            std::string err = make_json_result(false, true, "Engine not initialized", "", 0, false);
            return env->NewStringUTF(err.c_str());
        }

        const char* base64_c = env->GetStringUTFChars(base64_jstr, nullptr);
        std::string base64_str(base64_c);
        env->ReleaseStringUTFChars(base64_jstr, base64_c);

        cv::Mat frame = base64_to_mat(base64_str);
        if (frame.empty()) {
            std::string err = make_json_result(false, true, "Failed to decode image", "", 0, false);
            return env->NewStringUTF(err.c_str());
        }

        auto [liveness_state, liveness_msg, landmarks] = g_liveness->process_frame(frame);

        if (liveness_state == LivenessState::VERIFIED) {
            auto [person_id, confidence, recognition_msg] = g_face_engine->recognize_face(frame);
            if (!person_id.empty() && person_id != "Unknown") {
                g_storage->log(person_id, confidence);
                std::string result = make_json_result(true, false,
                    "Verified: " + person_id, person_id, confidence, true);
                return env->NewStringUTF(result.c_str());
            } else {
                std::string result = make_json_result(true, false,
                    "Access Denied: Subject Unknown", "", confidence, false);
                return env->NewStringUTF(result.c_str());
            }
        } else if (liveness_state == LivenessState::FAILED) {
            std::string result = make_json_result(false, true,
                "SPOOF ALERT: " + liveness_msg, "", 0, false);
            return env->NewStringUTF(result.c_str());
        } else {
            std::string result = make_json_result(false, false,
                liveness_msg, "", 0, false);
            return env->NewStringUTF(result.c_str());
        }
    } catch (const std::exception& e) {
        LOGE("nativeProcessBase64 error: %s", e.what());
        std::string err = make_json_result(false, true, e.what(), "", 0, false);
        return env->NewStringUTF(err.c_str());
    }
}

JNIEXPORT jstring JNICALL
Java_com_anonymous_hackathon_FaceAttendanceModule_nativeRegisterFace(
    JNIEnv* env, jobject thiz, jstring base64_jstr, jstring person_id_jstr) {
    try {
        if (!g_face_engine) {
            std::string err = "{\"success\":false,\"message\":\"Engine not initialized\"}";
            return env->NewStringUTF(err.c_str());
        }

        const char* base64_c = env->GetStringUTFChars(base64_jstr, nullptr);
        const char* person_id_c = env->GetStringUTFChars(person_id_jstr, nullptr);

        std::string base64_str(base64_c);
        std::string person_id_str(person_id_c);

        env->ReleaseStringUTFChars(base64_jstr, base64_c);
        env->ReleaseStringUTFChars(person_id_jstr, person_id_c);

        cv::Mat frame = base64_to_mat(base64_str);
        if (frame.empty()) {
            std::string err = "{\"success\":false,\"message\":\"Failed to decode image\"}";
            return env->NewStringUTF(err.c_str());
        }

        auto [success, message] = g_face_engine->register_face(frame, person_id_str);
        std::ostringstream json;
        json << "{\"success\":" << (success ? "true" : "false")
             << ",\"message\":\"" << message << "\"}";
        return env->NewStringUTF(json.str().c_str());
    } catch (const std::exception& e) {
        LOGE("nativeRegisterFace error: %s", e.what());
        std::string err = std::string("{\"success\":false,\"message\":\"") + e.what() + "\"}";
        return env->NewStringUTF(err.c_str());
    }
}

JNIEXPORT void JNICALL
Java_com_anonymous_hackathon_FaceAttendanceModule_nativeResetLiveness(
    JNIEnv* env, jobject thiz) {
    if (g_liveness) {
        g_liveness->reset();
        g_liveness->start();
        LOGI("Liveness reset");
    }
}

JNIEXPORT jstring JNICALL
Java_com_anonymous_hackathon_FaceAttendanceModule_nativeGetStats(
    JNIEnv* env, jobject thiz) {
    try {
        if (!g_storage) {
            return env->NewStringUTF("{\"total\":0,\"unsynced\":0,\"synced\":0}");
        }
        auto s = g_storage->stats();
        std::ostringstream json;
        json << "{\"total\":" << s.total
             << ",\"unsynced\":" << s.unsynced
             << ",\"synced\":" << s.synced << "}";
        return env->NewStringUTF(json.str().c_str());
    } catch (const std::exception& e) {
        return env->NewStringUTF("{\"error\":\"stats failed\"}");
    }
}

JNIEXPORT void JNICALL
Java_com_anonymous_hackathon_FaceAttendanceModule_nativeDestroy(
    JNIEnv* env, jobject thiz) {
    if (g_face_engine) { delete g_face_engine; g_face_engine = nullptr; }
    if (g_liveness) { delete g_liveness; g_liveness = nullptr; }
    if (g_storage) { delete g_storage; g_storage = nullptr; }
    LOGI("JNI nativeDestroy complete");
}

} // extern "C"
