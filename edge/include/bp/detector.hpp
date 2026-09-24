#pragma once

#include <vector>

#include "bp/types.hpp"

namespace bp {

// Punto di innesto del modello (es. YOLO): basta implementare detect().
class IDetector {
public:
    virtual ~IDetector() = default;

    // Restituisce le detection con coordinate NORMALIZZATE 0..1 rispetto al frame.
    // Non const: un detector vero puo' riusare buffer interni tra una chiamata e l'altra.
    virtual std::vector<Detection> detect(const Frame& frame) = 0;
};

} // namespace bp
