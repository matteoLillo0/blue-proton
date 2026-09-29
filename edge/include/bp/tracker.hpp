#pragma once

#include <vector>

#include "bp/types.hpp"

namespace bp {

// Assegna a ogni detection un track_id stabile nel tempo.
// Ha uno stato (le tracce aperte), per questo update() non e' const.
class ITracker {
public:
    virtual ~ITracker() = default;

    // Da chiamare una volta per frame, nell'ordine dei frame.
    // timestamp_s = Frame::timestamp_s del frame da cui vengono le detection: serve per
    // ragionare in secondi e non in frame, perche' gli fps cambiano (PC, scheda, video).
    virtual std::vector<TrackedDetection> update(const std::vector<Detection>& detections, double timestamp_s) = 0;
};

} // namespace bp
