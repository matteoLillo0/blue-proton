#pragma once

#include <chrono>
#include <memory>
#include <string>

#include "bp/frame_source.hpp"

namespace bp {

// Sorgente VERA: file video (mp4, avi, mkv...) letto con OpenCV (cv::VideoCapture).
// Compilata solo se CMake trova OpenCV (BP_HAVE_OPENCV).
//
// pace = true: consegna i frame alla velocita' del video, come farebbe una camera
//              (fps in status.json confrontabili con quelli sul campo).
// pace = false: il piu' veloce possibile, per misurare quanto regge la pipeline.
class OpenCvVideoSource final : public FrameSource {
public:
    // Lancia std::runtime_error se il file non si apre.
    explicit OpenCvVideoSource(const std::string& path, bool pace = true);
    ~OpenCvVideoSource() override;  // definito nel .cpp, dove Impl e' completo

    OpenCvVideoSource(const OpenCvVideoSource&) = delete;
    OpenCvVideoSource& operator=(const OpenCvVideoSource&) = delete;

    bool read(Frame& out) override;

    int width() const { return width_; }
    int height() const { return height_; }
    double fps() const { return fps_; }

private:
    using Clock = std::chrono::steady_clock;

    // Pimpl: gli header OpenCV restano nel .cpp, chi include questo file non ne ha bisogno.
    struct Impl;
    std::unique_ptr<Impl> impl_;

    std::string path_;
    bool pace_;
    int width_ = 0;
    int height_ = 0;
    double fps_ = 0.0;
    long frame_index_ = 0;
    Clock::time_point start_;
};

} // namespace bp
