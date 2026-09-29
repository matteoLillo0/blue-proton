#pragma once

#include "bp/detector.hpp"

namespace bp {

// Detector FINTO che imita un detector vero, per sviluppare tracker, server e dashboard.
// Ogni kSpawnEvery_s secondi un animale entra da un lato e attraversa la scena in
// kCrossTime_s secondi (alternando sinistra->destra e destra->sinistra). Come un modello
// vero, a volte non vede un animale, le box tremano un po' e ogni tanto compare un falso positivo.
//
// L'output dipende SOLO da frame.timestamp_s (niente stato, niente rand()): a parita' di
// input e' identico, quindi i test sanno quanti animali unici dovrebbero risultare.
class FakeDetector final : public IDetector {
public:
    static constexpr double kSpawnEvery_s = 4.0;
    static constexpr double kCrossTime_s = 12.0;

    std::vector<Detection> detect(const Frame& frame) override;

    // Quanti animali sono entrati in scena fino al tempo t (per i test).
    static int animals_entered(double t);
};

} // namespace bp
