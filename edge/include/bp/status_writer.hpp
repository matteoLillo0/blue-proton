#pragma once

#include "bp/types.hpp"

namespace bp {

// Pubblica lo stato della pipeline (oggi su file, domani magari su rete).
class StatusWriter {
public:
    virtual ~StatusWriter() = default;

    // Ritorna false se la scrittura fallisce: la pipeline non si deve fermare
    // per questo, chi chiama decide se segnalarlo.
    virtual bool write(const Status& status) = 0;
};

} // namespace bp
