#include "bp/fake_frame_source.hpp"

#include <thread>

namespace bp {

FakeFrameSource::FakeFrameSource(double fps, int width, int height)
    : period_(1.0 / fps),
      width_(width),
      height_(height),
      start_(Clock::now()),
      // Come una camera vera, il primo frame arriva dopo un periodo e non all'istante zero.
      next_frame_(start_ + std::chrono::duration_cast<Clock::duration>(period_)) {}

bool FakeFrameSource::read(Frame& out) {
    // Imitiamo una camera: read() blocca finche' non "arriva" il frame successivo.
    // sleep_until su una scadenza assoluta non accumula ritardo, sleep_for si'.
    std::this_thread::sleep_until(next_frame_);
    const auto now = Clock::now();
    next_frame_ += std::chrono::duration_cast<Clock::duration>(period_);
    // Se siamo molto in ritardo (es. processo sospeso) non recuperiamo a raffica.
    if (next_frame_ < now) {
        next_frame_ = now;
    }

    out.width = width_;
    out.height = height_;
    out.data.clear();  // frame finto: nessun pixel
    out.timestamp_s = std::chrono::duration<double>(now - start_).count();
    return true;  // una sorgente finta non finisce mai
}

} // namespace bp
