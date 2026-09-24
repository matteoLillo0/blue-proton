#pragma once

#include <chrono>

#include "bp/frame_source.hpp"

namespace bp {

// Sorgente FINTA: frame senza pixel a FPS fisso, per far girare la pipeline
// senza camera. Da sostituire con una sorgente vera (camera o file video).
class FakeFrameSource final : public FrameSource {
public:
    explicit FakeFrameSource(double fps = 10.0, int width = 640, int height = 480);
    bool read(Frame& out) override;

private:
    using Clock = std::chrono::steady_clock;  // monotono: non salta se cambia l'ora di sistema

    std::chrono::duration<double> period_;
    int width_;
    int height_;
    Clock::time_point start_;
    Clock::time_point next_frame_;
};

} // namespace bp
