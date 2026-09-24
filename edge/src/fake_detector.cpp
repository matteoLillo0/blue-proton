#include "bp/fake_detector.hpp"

#include <cmath>

namespace bp {

std::vector<Detection> FakeDetector::detect(const Frame& frame) {
    constexpr int kBoxes = 3;
    constexpr float kW = 0.15F;
    constexpr float kH = 0.20F;
    const auto t = static_cast<float>(frame.timestamp_s);

    std::vector<Detection> out;
    out.reserve(kBoxes);  // una sola allocazione
    for (int i = 0; i < kBoxes; ++i) {
        const auto fi = static_cast<float>(i);
        // Oscillazioni lente (periodo di decine di secondi), sfasate per box.
        // I centri restano abbastanza lontani dai bordi da non uscire da 0..1.
        const float cx = 0.2F + 0.3F * fi + 0.08F * std::sin(0.20F * t + fi);
        const float cy = 0.5F + 0.25F * std::sin(0.13F * t + 2.0F * fi);
        out.push_back(Detection{cx - kW / 2.0F, cy - kH / 2.0F, kW, kH, 0, 0.9F});
    }
    return out;
}

} // namespace bp
