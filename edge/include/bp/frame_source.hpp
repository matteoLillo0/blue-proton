#pragma once

#include "bp/types.hpp"

namespace bp {

// Qualunque sorgente di immagini: camera, file video, sorgente finta.
// Il main conosce solo questa interfaccia, quindi le sorgenti sono intercambiabili.
class FrameSource {
public:
    // Distruttore virtuale: distruggiamo le sorgenti tramite unique_ptr<FrameSource>,
    // senza questo il distruttore della classe derivata non verrebbe chiamato.
    virtual ~FrameSource() = default;

    // Riempie `out` con il prossimo frame. Ritorna false a fine stream o su errore.
    // Riceve un riferimento per poter riusare il buffer `data` tra un frame e l'altro.
    virtual bool read(Frame& out) = 0;
};

} // namespace bp
