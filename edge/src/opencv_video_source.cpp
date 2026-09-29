#include "bp/opencv_video_source.hpp"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <thread>

namespace bp {

namespace {

// Alcuni file non dichiarano gli fps (o dichiarano valori assurdi): usiamo un default ragionevole.
constexpr double kDefaultFps = 25.0;

} // namespace

struct OpenCvVideoSource::Impl {
    cv::VideoCapture cap;
    cv::Mat mat;  // riusato tra un frame e l'altro
};

OpenCvVideoSource::OpenCvVideoSource(const std::string& path, bool pace)
    : impl_(std::make_unique<Impl>()), path_(path), pace_(pace) {
    // Controllo esplicito: altrimenti OpenCV prova tutti i backend e stampa warning criptici.
    // Gli URL (rtsp://, http://...) non sono file e li lasciamo passare.
    if (path.find("://") == std::string::npos && !std::filesystem::is_regular_file(path)) {
        throw std::runtime_error(path + ": file non trovato");
    }
    if (!impl_->cap.open(path)) {
        throw std::runtime_error(path + ": impossibile aprire il video");
    }
    width_ = static_cast<int>(impl_->cap.get(cv::CAP_PROP_FRAME_WIDTH));
    height_ = static_cast<int>(impl_->cap.get(cv::CAP_PROP_FRAME_HEIGHT));
    fps_ = impl_->cap.get(cv::CAP_PROP_FPS);
    if (!(fps_ > 0.0 && fps_ <= 1000.0)) {
        fps_ = kDefaultFps;
    }
    start_ = Clock::now();
}

OpenCvVideoSource::~OpenCvVideoSource() = default;

bool OpenCvVideoSource::read(Frame& out) {
    if (!impl_->cap.read(impl_->mat) || impl_->mat.empty()) {
        return false;  // fine del file
    }
    cv::Mat& m = impl_->mat;

    // Il contratto vuole BGR 8 bit, 3 canali. I video normali lo sono gia';
    // gestiamo comunque grigio/BGRA per non passare dati sbagliati al detector.
    if (m.type() == CV_8UC1) {
        cv::cvtColor(m, m, cv::COLOR_GRAY2BGR);
    } else if (m.type() == CV_8UC4) {
        cv::cvtColor(m, m, cv::COLOR_BGRA2BGR);
    } else if (m.type() != CV_8UC3) {
        throw std::runtime_error(path_ + ": formato pixel non supportato");
    }

    // Tempo del frame nel video (non "adesso"): il tracker vede gli intervalli veri tra frame
    // anche se la pipeline va piu' lenta o piu' veloce del video.
    const double t = static_cast<double>(frame_index_) / fps_;
    ++frame_index_;

    if (pace_) {
        // Stessa logica della sorgente finta: scadenza assoluta, niente deriva.
        std::this_thread::sleep_until(start_ + std::chrono::duration_cast<Clock::duration>(
                                                   std::chrono::duration<double>(t)));
    }

    out.width = m.cols;
    out.height = m.rows;
    out.data.resize(static_cast<std::size_t>(m.cols) * m.rows * 3);
    if (m.isContinuous()) {
        std::memcpy(out.data.data(), m.data, out.data.size());
    } else {
        const std::size_t row_bytes = static_cast<std::size_t>(m.cols) * 3;
        for (int r = 0; r < m.rows; ++r) {
            std::memcpy(out.data.data() + r * row_bytes, m.ptr(r), row_bytes);
        }
    }
    out.timestamp_s = t;
    return true;
}

} // namespace bp
