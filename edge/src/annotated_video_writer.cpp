#include "bp/annotated_video_writer.hpp"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>

namespace bp {

struct AnnotatedVideoWriter::Impl {
    cv::VideoWriter writer;
    cv::Mat canvas;  // copia del frame su cui disegnare (il Frame resta intatto)
};

AnnotatedVideoWriter::AnnotatedVideoWriter(std::string path, double fps)
    : impl_(std::make_unique<Impl>()), path_(std::move(path)), fps_(fps) {}

AnnotatedVideoWriter::~AnnotatedVideoWriter() = default;

bool AnnotatedVideoWriter::write(const Frame& frame, const std::vector<TrackedDetection>& tracks) {
    if (frame.data.size() != static_cast<std::size_t>(frame.width) * frame.height * 3) {
        return false;
    }
    if (!impl_->writer.isOpened()) {
        // mp4v: codec MPEG-4 disponibile in qualunque build di OpenCV con FFmpeg.
        impl_->writer.open(path_, cv::VideoWriter::fourcc('m', 'p', '4', 'v'), fps_,
                           cv::Size(frame.width, frame.height));
        if (!impl_->writer.isOpened()) {
            return false;
        }
    }
    const cv::Mat src(frame.height, frame.width, CV_8UC3, const_cast<std::uint8_t*>(frame.data.data()));
    src.copyTo(impl_->canvas);
    cv::Mat& img = impl_->canvas;

    const int thickness = std::max(1, frame.width / 400);
    const double font = std::max(0.4, frame.width / 1600.0);
    for (const TrackedDetection& t : tracks) {
        // Coordinate normalizzate -> pixel (contratto, regola 1).
        const cv::Rect r(static_cast<int>(std::lround(t.det.x * frame.width)),
                         static_cast<int>(std::lround(t.det.y * frame.height)),
                         static_cast<int>(std::lround(t.det.w * frame.width)),
                         static_cast<int>(std::lround(t.det.h * frame.height)));
        const bool confirmed = t.track_id >= 0;
        const cv::Scalar color = confirmed ? cv::Scalar(0, 220, 0) : cv::Scalar(160, 160, 160);
        cv::rectangle(img, r, color, thickness);

        char label[32];
        if (confirmed) {
            std::snprintf(label, sizeof label, "#%d %.2f", t.track_id, static_cast<double>(t.det.confidence));
        } else {
            std::snprintf(label, sizeof label, "%.2f", static_cast<double>(t.det.confidence));
        }
        cv::putText(img, label, cv::Point(r.x, std::max(12, r.y - 4)), cv::FONT_HERSHEY_SIMPLEX, font, color,
                    thickness);
    }
    char ts[32];
    std::snprintf(ts, sizeof ts, "t=%.2fs", frame.timestamp_s);
    cv::putText(img, ts, cv::Point(8, frame.height - 10), cv::FONT_HERSHEY_SIMPLEX, font, cv::Scalar(255, 255, 255),
                thickness);

    impl_->writer.write(img);
    return true;
}

} // namespace bp
