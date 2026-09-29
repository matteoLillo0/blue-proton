// Test del tracker (IouTracker) e della pipeline detector simulato + tracker.

#include <algorithm>
#include <cmath>
#include <set>
#include <vector>

#include "check.hpp"
#include "fake_detector.hpp"
#include "bp/iou_tracker.hpp"

namespace {

bp::Detection box(float x, float y, float w = 0.1F, float h = 0.1F) { return bp::Detection{x, y, w, h, 0, 0.9F}; }

void test_iou() {
    CHECK(std::fabs(bp::iou(box(0.1F, 0.1F), box(0.1F, 0.1F)) - 1.0F) < 1e-6F);
    CHECK(bp::iou(box(0.1F, 0.1F), box(0.5F, 0.5F)) == 0.0F);
    // Spostata di mezza larghezza: intersezione 0.5, unione 1.5 -> 1/3.
    CHECK(std::fabs(bp::iou(box(0.0F, 0.0F), box(0.05F, 0.0F)) - 1.0F / 3.0F) < 1e-5F);
}

// Tracker con un orologio: simula una sorgente a fps fissi.
struct Sim {
    explicit Sim(double fps_ = 10.0, bp::IouTrackerParams p = {}) : tracker(p), fps(fps_) {}
    std::vector<bp::TrackedDetection> step(const std::vector<bp::Detection>& dets) {
        return tracker.update(dets, static_cast<double>(++frame) / fps);
    }
    bp::IouTracker tracker;
    double fps;
    long frame = 0;
};

void test_stable_id_and_confirmation() {
    Sim sim;  // min_hits = 3
    int id = -1;
    for (int f = 0; f < 30; ++f) {
        const auto out = sim.step({box(0.1F + 0.01F * f, 0.4F)});
        CHECK(out.size() == 1);
        if (f < 2) {
            CHECK(out[0].track_id == -1);  // non ancora confermata
        } else if (f == 2) {
            id = out[0].track_id;
            CHECK(id == 1);
        } else {
            CHECK(out[0].track_id == id);
        }
    }
}

void test_false_positive_never_gets_id() {
    Sim sim;
    for (int f = 0; f < 20; ++f) {
        // Un box che compare in un punto diverso a ogni frame (come un falso positivo).
        const auto out = sim.step({box(0.05F * static_cast<float>(f % 10), 0.05F * static_cast<float>(f % 7) * 2)});
        CHECK(out[0].track_id == -1);
    }
}

void test_missed_frames() {
    bp::IouTrackerParams p;
    p.max_lost_s = 0.55;  // a 10 fps: 5 frame persi si', 6 no
    Sim sim(10.0, p);
    float x = 0.1F;
    int id = -1;
    for (int f = 0; f < 5; ++f, x += 0.01F) {
        id = sim.step({box(x, 0.4F)})[0].track_id;
    }
    CHECK(id > 0);
    // 5 frame senza detection: la traccia sopravvive e la previsione la tiene al passo
    // con l'animale che continua a muoversi.
    for (int f = 0; f < 5; ++f, x += 0.01F) {
        CHECK(sim.step({}).empty());
    }
    CHECK(sim.step({box(x, 0.4F)})[0].track_id == id);
    x += 0.01F;
    // 6 frame senza detection: traccia chiusa, poi serve un id nuovo.
    for (int f = 0; f < 6; ++f, x += 0.01F) {
        sim.step({});
    }
    int new_id = -1;
    for (int f = 0; f < 3; ++f, x += 0.01F) {
        new_id = sim.step({box(x, 0.4F)})[0].track_id;
    }
    CHECK(new_id > 0);
    CHECK(new_id != id);
}

void test_tolerance_is_in_seconds() {
    // Lo stesso buco di 1.5 s senza detection (max_lost_s = 2 s) deve mantenere l'id
    // sia a 30 fps (45 frame) sia a 3 fps (4-5 frame): e' questo il motivo dei secondi.
    for (const double fps : {3.0, 30.0}) {
        Sim sim(fps);
        const float speed = 0.05F;  // unita' normalizzate al secondo
        auto x_at = [&](long frame) { return 0.2F + speed * static_cast<float>(frame / fps); };
        int id = -1;
        for (int f = 0; f < 5; ++f) {
            id = sim.step({box(x_at(sim.frame + 1), 0.4F, 0.15F, 0.15F)})[0].track_id;
        }
        const long gap = std::lround(1.5 * fps);
        for (long f = 0; f < gap; ++f) {
            sim.step({});
        }
        const int after = sim.step({box(x_at(sim.frame + 1), 0.4F, 0.15F, 0.15F)})[0].track_id;
        if (after != id) {
            std::cerr << "  fps=" << fps << ": id " << id << " diventato " << after << '\n';
        }
        CHECK(id > 0 && after == id);
    }
}

void test_exit_and_enter_same_side() {
    // Un animale esce da sinistra e, 0.5 s dopo, un altro entra da sinistra nella stessa
    // corsia: deve ricevere un id NUOVO (0.5 s < max_lost_s, ma la traccia uscita era sul bordo).
    Sim sim;
    int old_id = -1;
    for (int f = 0; f < 10; ++f) {
        const float x = 0.1F - 0.012F * static_cast<float>(f);
        old_id = sim.step({box(std::max(0.0F, x), 0.4F, 0.1F - std::max(0.0F, -x))})[0].track_id;
    }
    CHECK(old_id > 0);
    for (int f = 0; f < 5; ++f) {
        sim.step({});
    }
    int new_id = -1;
    for (int f = 0; f < 5; ++f) {
        new_id = sim.step({box(0.0F, 0.4F, 0.03F + 0.01F * static_cast<float>(f))})[0].track_id;
    }
    CHECK(new_id > 0);
    CHECK(new_id != old_id);
}

void test_crossing_keeps_ids() {
    // Due animali in direzioni opposte, corsie vicine: si sovrappongono per diversi frame.
    Sim sim;
    int id_a = -1;
    int id_b = -1;
    for (int f = 0; f < 60; ++f) {
        const float t = static_cast<float>(f);
        const auto out = sim.step({box(0.1F + 0.012F * t, 0.40F), box(0.8F - 0.012F * t, 0.45F)});
        CHECK(out.size() == 2);  // stesso ordine e numero delle detection
        if (f == 2) {
            id_a = out[0].track_id;
            id_b = out[1].track_id;
            CHECK(id_a > 0 && id_b > 0 && id_a != id_b);
        } else if (f > 2) {
            CHECK(out[0].track_id == id_a);
            CHECK(out[1].track_id == id_b);
        }
    }
}

// Pipeline completa con il detector simulato: detection mancate, tremolio e falsi positivi
// non devono cambiare il numero di animali unici, a qualunque fps.
void test_unique_count_with_fake_detector(double fps) {
    bp::FakeDetector detector;
    bp::IouTracker tracker;
    std::set<int> ids;
    // L'ultimo animale entra a t=200 s; a 202 s e' in scena da abbastanza per essere confermato.
    const double end_s = 202.0;
    bp::Frame frame;
    for (long i = 1; static_cast<double>(i) / fps <= end_s; ++i) {
        frame.timestamp_s = static_cast<double>(i) / fps;
        for (const auto& t : tracker.update(detector.detect(frame), frame.timestamp_s)) {
            if (t.track_id >= 0) {
                ids.insert(t.track_id);
            }
        }
    }
    const int expected = bp::FakeDetector::animals_entered(end_s);
    if (static_cast<int>(ids.size()) != expected) {
        std::cerr << "  fps=" << fps << ": attesi " << expected << " animali unici, contati " << ids.size() << '\n';
    }
    CHECK(static_cast<int>(ids.size()) == expected);
}

} // namespace

void run_tracker_tests() {
    test_iou();
    test_stable_id_and_confirmation();
    test_false_positive_never_gets_id();
    test_missed_frames();
    test_tolerance_is_in_seconds();
    test_exit_and_enter_same_side();
    test_crossing_keeps_ids();
    test_unique_count_with_fake_detector(10.0);
    test_unique_count_with_fake_detector(25.0);
    test_unique_count_with_fake_detector(3.0);  // velocita' realistica sulla UNO Q
}
