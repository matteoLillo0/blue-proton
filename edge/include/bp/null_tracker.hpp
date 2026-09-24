#pragma once

#include "bp/tracker.hpp"

namespace bp {

// Tracker FINTO: da' a ogni detection un id nuovo e progressivo.
// Gli id NON sono stabili tra frame: serve solo a far girare la pipeline
// finche' non c'e' il tracker vero.
class NullTracker final : public ITracker {
public:
    std::vector<TrackedDetection> update(const std::vector<Detection>& detections) override;

private:
    int next_id_ = 1;
};

} // namespace bp
