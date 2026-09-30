#pragma once

#include <chrono>
#include <memory>
#include <string>

#include "bp/frame_source.hpp"

namespace bp {

// Richiesta alla camera di OpenCvVideoSource, usata solo con /dev/videoN
// (0 = lascia decidere alla camera). Fuori dalla classe: C++ non permette di usarla
// come argomento di default del costruttore se e' annidata.
struct CameraRequest {
    int width = 0;
    int height = 0;
    double fps = 0.0;
};

// Sorgente VERA letta con OpenCV (cv::VideoCapture). Accetta:
// - file video (mp4, webm, mkv...);
// - sorgenti DAL VIVO: camera (/dev/videoN, anche solo MJPEG: la decodifica OpenCV),
//   pipeline GStreamer (contiene '!', deve finire con "! appsink") o URL (rtsp://, http://).
//
// File, pace = true: consegna i frame alla velocita' del video, come farebbe una camera
//                    (fps in status.json confrontabili con quelli sul campo).
// File, pace = false: il piu' veloce possibile, per misurare quanto regge la pipeline.
// Dal vivo: pace ignorato, il timestamp e' l'istante di lettura.
class OpenCvVideoSource final : public FrameSource {
public:
    // Lancia std::runtime_error se la sorgente non si apre.
    explicit OpenCvVideoSource(const std::string& path, bool pace = true, CameraRequest request = {});
    ~OpenCvVideoSource() override;  // definito nel .cpp, dove Impl e' completo

    OpenCvVideoSource(const OpenCvVideoSource&) = delete;
    OpenCvVideoSource& operator=(const OpenCvVideoSource&) = delete;

    bool read(Frame& out) override;

    int width() const { return width_; }
    int height() const { return height_; }
    double fps() const { return fps_; }
    bool live() const { return live_; }

private:
    using Clock = std::chrono::steady_clock;

    // Pimpl: gli header OpenCV restano nel .cpp, chi include questo file non ne ha bisogno.
    struct Impl;
    std::unique_ptr<Impl> impl_;

    std::string path_;
    bool pace_;
    bool live_ = false;
    int width_ = 0;
    int height_ = 0;
    double fps_ = 0.0;
    long frame_index_ = 0;
    Clock::time_point start_;
};

} // namespace bp
