#pragma once

#include "bp/detector.hpp"

namespace bp {

// Detector FINTO: 3 box che si muovono lentamente.
// La posizione dipende solo da frame.timestamp_s, quindi a parita' di input
// l'output e' identico: utile per sviluppare dashboard e server.
class FakeDetector final : public IDetector {
public:
    std::vector<Detection> detect(const Frame& frame) override;
};

} // namespace bp
