#pragma once

// Tipi di dati condivisi da tutto il team. Regole in docs/contract.md:
// non cambiarli senza avvisare gli altri.

#include <cstdint>
#include <vector>

namespace bp {

// Un'immagine catturata.
// data: pixel in righe contigue, 3 byte per pixel in ordine BGR
// (size = width * height * 3). Puo' essere vuoto nelle sorgenti finte.
struct Frame {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> data;
    double timestamp_s = 0.0;  // secondi dall'avvio della sorgente (orologio monotono)
};

// Un oggetto rilevato.
// x, y, w, h sono NORMALIZZATI 0..1 rispetto al frame (x, y = angolo alto-sinistra):
// cosi' tracker e dashboard non dipendono dalla risoluzione di camera o modello.
struct Detection {
    float x = 0.0F;
    float y = 0.0F;
    float w = 0.0F;
    float h = 0.0F;
    int class_id = 0;
    float confidence = 0.0F;
};

// Detection + identita' assegnata dal tracker.
// Composizione e non ereditarieta': il detector non sa nulla del tracking,
// e non si rischia di "perdere" il track_id copiando in una Detection.
struct TrackedDetection {
    Detection det;
    int track_id = -1;  // -1 = non ancora assegnato
};

// Contenuto di status.json. Se cambia il significato di un campo
// va incrementato kSchemaVersion, cosi' il server se ne accorge.
struct Status {
    static constexpr int kSchemaVersion = 1;
    double timestamp = 0.0;  // secondi Unix (UTC) al momento della scrittura
    double fps = 0.0;        // frame elaborati al secondo, sull'ultimo secondo
    int count = 0;           // oggetti tracciati nell'ultimo frame
};

} // namespace bp
