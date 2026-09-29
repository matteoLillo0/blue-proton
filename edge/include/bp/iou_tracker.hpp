#pragma once

#include <vector>

#include "bp/geometry.hpp"
#include "bp/tracker.hpp"

namespace bp {

// Parametri del tracker. Le durate sono in SECONDI: valgono uguali a 30 fps su PC
// e a pochi fps sulla scheda.
struct IouTrackerParams {
    // Sovrapposizione minima (IoU) tra traccia prevista e detection per considerarle
    // lo stesso animale. Piu' bassa = regge movimenti veloci, ma rischia scambi di id.
    float iou_threshold = 0.3F;
    // Frame consecutivi in cui una traccia deve essere vista prima di ricevere un id.
    // Filtra i falsi positivi isolati, che altrimenti gonfierebbero il conteggio.
    int min_hits = 3;
    // Per quanto una traccia resta aperta senza detection. Copre le detection mancate
    // (animale lontano, poca luce) e le brevi occlusioni; troppo alto = un animale nuovo
    // nello stesso punto puo' "ereditare" un id vecchio.
    double max_lost_s = 2.0;
    // Come max_lost_s, ma per le tracce che toccano il bordo dell'immagine: se spariscono
    // li' sono quasi certamente uscite. Chiuderle presto evita che un animale che entra
    // dallo stesso lato erediti l'id di quello appena uscito. Rischio opposto: un animale
    // fermo sul bordo e non visto per piu' di cosi' riceve un id nuovo.
    double max_lost_at_edge_s = 0.4;
    // Distanza dal bordo (normalizzata) entro cui una box "tocca" il bordo.
    float edge_margin = 0.01F;
};

// Tracker VERO, semplice e senza dipendenze (stile SORT senza filtro di Kalman):
// 1. prevede dove sara' ogni traccia aperta con la sua velocita' media;
// 2. abbina tracce previste e detection per IoU decrescente (greedy);
// 3. aggiorna le tracce abbinate, apre tracce nuove per le detection rimaste,
//    chiude quelle non viste da troppo.
// Gli id partono da 1, crescono e non vengono mai riusati: quanti id diversi sono
// comparsi = quanti animali unici sono stati visti.
class IouTracker final : public ITracker {
public:
    explicit IouTracker(IouTrackerParams params = IouTrackerParams{});

    // Restituisce una TrackedDetection per ogni detection, nello STESSO ordine.
    // track_id = -1 finche' la traccia non e' confermata (vedi min_hits).
    std::vector<TrackedDetection> update(const std::vector<Detection>& detections, double timestamp_s) override;

private:
    struct Track {
        Detection box;             // ultima posizione VISTA (non la previsione)
        float vx = 0.0F;           // velocita' media in unita' normalizzate al secondo
        float vy = 0.0F;
        double last_seen_s = 0.0;  // timestamp dell'ultima detection abbinata
        int id = -1;               // -1 finche' non confermata
        int hits = 0;              // frame consecutivi in cui e' stata vista
        bool seen_now = false;     // abbinata nel frame corrente
    };

    // Posizione prevista della traccia all'istante timestamp_s.
    static Detection predict(const Track& t, double timestamp_s);

    IouTrackerParams params_;
    std::vector<Track> tracks_;
    int next_id_ = 1;
};

} // namespace bp
