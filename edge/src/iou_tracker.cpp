#include "bp/iou_tracker.hpp"

#include <algorithm>
#include <cstddef>

namespace bp {

namespace {

// Peso della nuova misura nella media della velocita': piu' basso = velocita' piu' liscia
// ma lenta ad adattarsi ai cambi di direzione.
constexpr float kVelocitySmoothing = 0.5F;

struct Candidate {
    float iou;
    std::size_t track;
    std::size_t det;
};

} // namespace

IouTracker::IouTracker(IouTrackerParams params) : params_(params) {}

Detection IouTracker::predict(const Track& t, double timestamp_s) {
    const auto dt = static_cast<float>(timestamp_s - t.last_seen_s);
    Detection p = t.box;
    p.x += t.vx * dt;
    p.y += t.vy * dt;
    return p;
}

std::vector<TrackedDetection> IouTracker::update(const std::vector<Detection>& detections, double timestamp_s) {
    // 1. Previsione: dove dovrebbe essere adesso ogni traccia, data la sua velocita'.
    std::vector<Detection> predicted;
    predicted.reserve(tracks_.size());
    for (Track& t : tracks_) {
        predicted.push_back(predict(t, timestamp_s));
        t.seen_now = false;
    }

    // 2. Abbinamento greedy: prima le coppie piu' sovrapposte. Con pochi animali per frame
    //    da' quasi sempre lo stesso risultato dell'algoritmo ungherese, ed e' molto piu' semplice.
    std::vector<Candidate> candidates;
    for (std::size_t ti = 0; ti < tracks_.size(); ++ti) {
        for (std::size_t di = 0; di < detections.size(); ++di) {
            const float v = iou(predicted[ti], detections[di]);
            if (v >= params_.iou_threshold) {
                candidates.push_back({v, ti, di});
            }
        }
    }
    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate& a, const Candidate& b) { return a.iou > b.iou; });

    std::vector<int> det_to_track(detections.size(), -1);
    for (const Candidate& c : candidates) {
        if (tracks_[c.track].seen_now || det_to_track[c.det] != -1) {
            continue;
        }
        tracks_[c.track].seen_now = true;
        det_to_track[c.det] = static_cast<int>(c.track);
    }

    // 3a. Tracce abbinate: nuova posizione, velocita' aggiornata, eventuale conferma.
    for (std::size_t di = 0; di < detections.size(); ++di) {
        if (det_to_track[di] < 0) {
            continue;
        }
        const auto ti = static_cast<std::size_t>(det_to_track[di]);
        Track& t = tracks_[ti];
        const Detection& d = detections[di];
        // Errore della previsione diviso il tempo trascorso = correzione della velocita'.
        const auto dt = static_cast<float>(timestamp_s - t.last_seen_s);
        if (dt > 0.0F) {
            t.vx += kVelocitySmoothing * (d.x - predicted[ti].x) / dt;
            t.vy += kVelocitySmoothing * (d.y - predicted[ti].y) / dt;
        }
        t.box = d;
        t.last_seen_s = timestamp_s;
        ++t.hits;
        if (t.id < 0 && t.hits >= params_.min_hits) {
            t.id = next_id_++;
        }
    }

    // Output prima di aggiungere/togliere tracce: det_to_track contiene indici in tracks_.
    std::vector<TrackedDetection> out;
    out.reserve(detections.size());
    for (std::size_t di = 0; di < detections.size(); ++di) {
        const int ti = det_to_track[di];
        out.push_back(TrackedDetection{detections[di], ti >= 0 ? tracks_[static_cast<std::size_t>(ti)].id : -1});
    }

    // 3b. Chiusura: le tracce non confermate che saltano un frame (probabili falsi positivi)
    //     e quelle confermate non viste da troppo (meno tolleranza sul bordo: l'animale e' uscito).
    const float m = params_.edge_margin;
    tracks_.erase(std::remove_if(tracks_.begin(), tracks_.end(),
                                 [&](const Track& t) {
                                     if (t.seen_now) {
                                         return false;
                                     }
                                     if (t.id < 0) {
                                         return true;
                                     }
                                     const Detection& b = t.box;
                                     const bool at_edge = b.x <= m || b.y <= m || b.x + b.w >= 1.0F - m ||
                                                          b.y + b.h >= 1.0F - m;
                                     const double lost_s = timestamp_s - t.last_seen_s;
                                     return lost_s > (at_edge ? params_.max_lost_at_edge_s : params_.max_lost_s);
                                 }),
                  tracks_.end());

    // 3c. Detection senza traccia: nuova traccia candidata. Se min_hits <= 1 e' subito confermata.
    for (std::size_t di = 0; di < detections.size(); ++di) {
        if (det_to_track[di] >= 0) {
            continue;
        }
        Track t;
        t.box = detections[di];
        t.last_seen_s = timestamp_s;
        t.hits = 1;
        if (params_.min_hits <= 1) {
            t.id = next_id_++;
            out[di].track_id = t.id;
        }
        tracks_.push_back(t);
    }
    return out;
}

} // namespace bp
