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

float iou(const Detection& a, const Detection& b) {
    const float ix = std::max(0.0F, std::min(a.x + a.w, b.x + b.w) - std::max(a.x, b.x));
    const float iy = std::max(0.0F, std::min(a.y + a.h, b.y + b.h) - std::max(a.y, b.y));
    const float inter = ix * iy;
    const float uni = a.w * a.h + b.w * b.h - inter;
    return uni > 0.0F ? inter / uni : 0.0F;
}

IouTracker::IouTracker(IouTrackerParams params) : params_(params) {}

std::vector<TrackedDetection> IouTracker::update(const std::vector<Detection>& detections) {
    // 1. Previsione: ogni traccia si sposta della sua velocita' (un frame).
    for (Track& t : tracks_) {
        t.box.x += t.vx;
        t.box.y += t.vy;
    }

    // 2. Abbinamento greedy: prima le coppie piu' sovrapposte. Con pochi animali per frame
    //    da' quasi sempre lo stesso risultato dell'algoritmo ungherese, ed e' molto piu' semplice.
    std::vector<Candidate> candidates;
    for (std::size_t ti = 0; ti < tracks_.size(); ++ti) {
        for (std::size_t di = 0; di < detections.size(); ++di) {
            const float v = iou(tracks_[ti].box, detections[di]);
            if (v >= params_.iou_threshold) {
                candidates.push_back({v, ti, di});
            }
        }
    }
    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate& a, const Candidate& b) { return a.iou > b.iou; });

    std::vector<bool> track_used(tracks_.size(), false);
    std::vector<int> det_to_track(detections.size(), -1);
    for (const Candidate& c : candidates) {
        if (track_used[c.track] || det_to_track[c.det] != -1) {
            continue;
        }
        track_used[c.track] = true;
        det_to_track[c.det] = static_cast<int>(c.track);
    }

    // 3a. Tracce abbinate: nuova posizione, velocita' aggiornata, eventuale conferma.
    for (std::size_t di = 0; di < detections.size(); ++di) {
        if (det_to_track[di] < 0) {
            continue;
        }
        Track& t = tracks_[static_cast<std::size_t>(det_to_track[di])];
        const Detection& d = detections[di];
        // t.box e' la posizione PREVISTA: la differenza e' l'errore di velocita' per frame.
        // Dopo n frame persi la previsione e' andata avanti n+1 volte: dividiamo per n+1.
        const auto frames = static_cast<float>(t.misses + 1);
        t.vx += kVelocitySmoothing * (d.x - t.box.x) / frames;
        t.vy += kVelocitySmoothing * (d.y - t.box.y) / frames;
        t.box = d;
        t.misses = 0;
        ++t.hits;
        if (t.id < 0 && t.hits >= params_.min_hits) {
            t.id = next_id_++;
        }
    }

    // 3b. Tracce non viste: restano aperte (con la posizione prevista) per max_misses frame.
    for (std::size_t ti = 0; ti < tracks_.size(); ++ti) {
        if (!track_used[ti]) {
            ++tracks_[ti].misses;
        }
    }

    // Output prima di aggiungere/togliere tracce: det_to_track contiene indici in tracks_.
    std::vector<TrackedDetection> out;
    out.reserve(detections.size());
    for (std::size_t di = 0; di < detections.size(); ++di) {
        const int ti = det_to_track[di];
        out.push_back(TrackedDetection{detections[di], ti >= 0 ? tracks_[static_cast<std::size_t>(ti)].id : -1});
    }

    // 3c. Chiusura: via le tracce perse da troppo (meno tolleranza sul bordo: probabilmente
    //     l'animale e' uscito) e quelle non confermate che hanno perso il filo.
    const float m = params_.edge_margin;
    tracks_.erase(std::remove_if(tracks_.begin(), tracks_.end(),
                                 [&](const Track& t) {
                                     if (t.id < 0) {
                                         return t.misses > 0;
                                     }
                                     const Detection& b = t.box;
                                     const bool at_edge = b.x <= m || b.y <= m || b.x + b.w >= 1.0F - m ||
                                                          b.y + b.h >= 1.0F - m;
                                     return t.misses > (at_edge ? params_.max_misses_at_edge : params_.max_misses);
                                 }),
                  tracks_.end());

    // 3d. Detection senza traccia: nuova traccia candidata. Se min_hits <= 1 e' subito confermata.
    for (std::size_t di = 0; di < detections.size(); ++di) {
        if (det_to_track[di] >= 0) {
            continue;
        }
        Track t;
        t.box = detections[di];
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
