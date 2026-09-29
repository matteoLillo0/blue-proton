#pragma once

#include <vector>

#include "bp/tracker.hpp"

namespace bp {

// Parametri del tracker. Le durate sono in FRAME (ITracker non riceve il tempo):
// se cambia molto l'fps della sorgente vanno riadattate.
struct IouTrackerParams {
    // Sovrapposizione minima (IoU) tra traccia prevista e detection per considerarle
    // lo stesso animale. Piu' bassa = regge movimenti veloci, ma rischia scambi di id.
    float iou_threshold = 0.3F;
    // Frame consecutivi in cui una traccia deve essere vista prima di ricevere un id.
    // Filtra i falsi positivi di un frame solo, che altrimenti gonfierebbero il conteggio.
    int min_hits = 3;
    // Frame di fila senza detection dopo cui la traccia viene chiusa. Copre le detection
    // mancate e le brevi occlusioni; troppo alto = un animale nuovo puo' "ereditare" un id vecchio.
    int max_misses = 10;
    // Come max_misses, ma per le tracce che toccano il bordo dell'immagine: se spariscono
    // li' sono quasi certamente uscite. Chiuderle presto evita che un animale che entra
    // dallo stesso lato "erediti" l'id di quello appena uscito. Rischio opposto: un animale
    // fermo sul bordo e non visto per piu' di questi frame riceve un id nuovo.
    int max_misses_at_edge = 3;
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
    std::vector<TrackedDetection> update(const std::vector<Detection>& detections) override;

private:
    struct Track {
        Detection box;       // ultima posizione nota o prevista
        float vx = 0.0F;     // velocita' media in unita' normalizzate per frame
        float vy = 0.0F;
        int id = -1;         // -1 finche' non confermata
        int hits = 0;        // frame consecutivi in cui e' stata vista
        int misses = 0;      // frame consecutivi in cui NON e' stata vista
    };

    IouTrackerParams params_;
    std::vector<Track> tracks_;
    int next_id_ = 1;
};

// Intersection over Union di due box: 0 = disgiunte, 1 = identiche.
float iou(const Detection& a, const Detection& b);

} // namespace bp
