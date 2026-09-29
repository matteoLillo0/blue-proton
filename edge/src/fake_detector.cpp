#include "bp/fake_detector.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace bp {

namespace {

constexpr float kW = 0.12F;               // dimensioni box (normalizzate)
constexpr float kH = 0.15F;
constexpr float kJitter = 0.004F;         // tremolio massimo della box, per lato
constexpr float kMissRate = 0.10F;        // probabilita' di non vedere un animale in un frame
constexpr float kFalsePositiveRate = 0.03F;
constexpr float kMinVisibleW = 0.03F;     // sotto questa larghezza (animale sul bordo) non si vede

// Numero pseudo-casuale in [0, 1) calcolato da due interi: stesso input, stesso output.
// Sostituisce rand() per avere un detector senza stato e ripetibile.
float rand01(std::uint32_t a, std::uint32_t b) {
    std::uint32_t h = a * 0x9E3779B1U ^ (b + 0x7F4A7C15U) * 0x85EBCA77U;
    h ^= h >> 15;
    h *= 0x2C1B3C6DU;
    h ^= h >> 12;
    h *= 0x297A2D39U;
    h ^= h >> 15;
    return static_cast<float>(h >> 8) * (1.0F / 16777216.0F);  // 24 bit -> [0, 1)
}

} // namespace

int FakeDetector::animals_entered(double t) {
    return t < 0.0 ? 0 : static_cast<int>(std::floor(t / kSpawnEvery_s)) + 1;
}

std::vector<Detection> FakeDetector::detect(const Frame& frame) {
    const double t = frame.timestamp_s;
    // "Seme" del frame: diverso a ogni frame, identico se il timestamp e' lo stesso.
    const auto tick = static_cast<std::uint32_t>(std::llround(t * 1000.0));

    std::vector<Detection> out;
    const int last = animals_entered(t) - 1;
    const int first = std::max(0, last - static_cast<int>(std::ceil(kCrossTime_s / kSpawnEvery_s)));
    for (int k = first; k <= last; ++k) {
        const double age = t - k * kSpawnEvery_s;
        if (age < 0.0 || age > kCrossTime_s) {
            continue;  // non ancora entrato o gia' uscito
        }
        const auto ku = static_cast<std::uint32_t>(k);
        if (rand01(ku, tick) < kMissRate) {
            continue;  // il "modello" non lo vede in questo frame
        }
        // Parte appena fuori dal bordo e arriva appena fuori dal bordo opposto.
        const auto progress = static_cast<float>(age / kCrossTime_s);
        const float travel = progress * (1.0F + kW);
        float x = (k % 2 == 0) ? -kW + travel : 1.0F - travel;
        float y = 0.1F + 0.65F * rand01(ku, 0xA11CEU);  // corsia fissa per animale
        x += (rand01(ku, tick + 1) - 0.5F) * 2.0F * kJitter;
        y += (rand01(ku, tick + 2) - 0.5F) * 2.0F * kJitter;

        // Contratto: box ritagliate dentro 0..1.
        const float x0 = std::max(0.0F, x);
        const float x1 = std::min(1.0F, x + kW);
        if (x1 - x0 < kMinVisibleW) {
            continue;
        }
        out.push_back(Detection{x0, y, x1 - x0, kH, 0, 0.6F + 0.35F * rand01(ku, tick + 3)});
    }

    // Falso positivo: una box in un punto a caso per un solo frame.
    if (rand01(0xFA15EU, tick) < kFalsePositiveRate) {
        const float fx = 0.9F * rand01(0xFA15EU, tick + 1);
        const float fy = 0.9F * rand01(0xFA15EU, tick + 2);
        out.push_back(Detection{fx, fy, 0.08F, 0.08F, 0, 0.35F});
    }
    return out;
}

} // namespace bp
