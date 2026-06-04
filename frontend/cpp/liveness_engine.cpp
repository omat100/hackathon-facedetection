#include <iostream>
#include <vector>
#include <cmath>
#include <string>
#include <deque>
#include <sstream>

// ─── Structs ──────────────────────────────────────────────────────────────────

struct FaceMetrics {
    float avg_ear;
    float smile_ratio;
    float turn_ratio;
};

struct BoundingBox {
    float x1, y1, x2, y2;
    float area() const {
        return std::abs(x2 - x1) * std::abs(y2 - y1);
    }
};

// ─── Circular Buffer ─────────────────────────────────────────────────────────

struct EARBuffer {
    std::deque<float> data;
    int capacity;

    EARBuffer(int cap) : capacity(cap) {}

    void push(float val) {
        data.push_back(val);
        if ((int)data.size() > capacity)
            data.pop_front();
    }

    bool full() const { return (int)data.size() == capacity; }

    bool all_below(float threshold) const {
        if (!full()) return false;
        for (float v : data)
            if (v >= threshold) return false;
        return true;
    }
};

// ─── Liveness States ─────────────────────────────────────────────────────────

enum class LivenessState {
    CHECKING =  1,
    VERIFIED =  4,
    FAILED   = -1
};

// ─── Liveness Engine ─────────────────────────────────────────────────────────

class LivenessEngine {
public:
    const float EAR_THRESH   = 0.20f;
    const float SMILE_THRESH = 0.48f;
    const float TURN_LOW     = 0.35f;
    const float TURN_HIGH    = 0.65f;
    const int   MAX_FRAMES   = 150;

    EARBuffer ear_buffer{5};
    int frame_counter = 0;

    std::pair<LivenessState, std::string> process(const FaceMetrics& m) {
        frame_counter++;
        ear_buffer.push(m.avg_ear);

        if (frame_counter > MAX_FRAMES)
            return {LivenessState::FAILED, "FAILED TIMEOUT"};

        if (ear_buffer.all_below(EAR_THRESH))
            return {LivenessState::VERIFIED, "VERIFIED BLINK"};

        if (m.smile_ratio > SMILE_THRESH)
            return {LivenessState::VERIFIED, "VERIFIED SMILE"};

        if (m.turn_ratio < TURN_LOW || m.turn_ratio > TURN_HIGH)
            return {LivenessState::VERIFIED, "VERIFIED TURN"};

        return {LivenessState::CHECKING, "CHECKING"};
    }
};

// ─── Multi-face selector ──────────────────────────────────────────────────────

int select_largest_face(const std::vector<BoundingBox>& boxes) {
    if (boxes.empty()) return -1;
    int   best_idx  = 0;
    float best_area = boxes[0].area();
    for (int i = 1; i < (int)boxes.size(); i++) {
        float a = boxes[i].area();
        if (a > best_area) {
            best_area = a;
            best_idx  = i;
        }
    }
    return best_idx;
}

// ─── Main ─────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    // Mode 1: liveness (default)
    // Mode 2: face_select — picks largest face from bounding boxes
    std::string mode = "liveness";
    if (argc > 1) mode = argv[1];

    if (mode == "face_select") {
        // Each line: "n x1 y1 x2 y2 x1 y1 x2 y2 ..." (n boxes)
        std::string line;
        while (std::getline(std::cin, line)) {
            std::istringstream ss(line);
            int n;
            ss >> n;
            std::vector<BoundingBox> boxes;
            for (int i = 0; i < n; i++) {
                BoundingBox b;
                ss >> b.x1 >> b.y1 >> b.x2 >> b.y2;
                boxes.push_back(b);
            }
            std::cout << select_largest_face(boxes) << std::endl;
            std::cout.flush();
        }
    } else {
        // Liveness mode
        LivenessEngine engine;
        float avg_ear, smile_ratio, turn_ratio;
        while (std::cin >> avg_ear >> smile_ratio >> turn_ratio) {
            FaceMetrics m{avg_ear, smile_ratio, turn_ratio};
            auto [state, result] = engine.process(m);
            std::cout << result << std::endl;
            std::cout.flush();
            if (state != LivenessState::CHECKING)
                break;
        }
    }

    return 0;
}