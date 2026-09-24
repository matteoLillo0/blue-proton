#include "bp/null_tracker.hpp"

namespace bp {

std::vector<TrackedDetection> NullTracker::update(const std::vector<Detection>& detections) {
    std::vector<TrackedDetection> out;
    out.reserve(detections.size());
    for (const Detection& d : detections) {
        out.push_back(TrackedDetection{d, next_id_++});
    }
    return out;
}

} // namespace bp
