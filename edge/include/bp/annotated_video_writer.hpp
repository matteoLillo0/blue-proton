#pragma once

#include <memory>
#include <string>
#include <vector>

#include "bp/types.hpp"

namespace bp {

// Strumento di DEBUG: scrive un video con le box disegnate sopra i frame, per vedere
// a occhio cosa rileva il detector e come il tracker assegna gli id.
// Verde con "#id" = traccia confermata (contata); grigio = non ancora confermata.
class AnnotatedVideoWriter {
public:
    // Il file viene aperto al primo frame (serve conoscerne la dimensione).
    AnnotatedVideoWriter(std::string path, double fps);
    ~AnnotatedVideoWriter();

    AnnotatedVideoWriter(const AnnotatedVideoWriter&) = delete;
    AnnotatedVideoWriter& operator=(const AnnotatedVideoWriter&) = delete;

    // Ritorna false se il video non si puo' scrivere.
    bool write(const Frame& frame, const std::vector<TrackedDetection>& tracks);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::string path_;
    double fps_;
};

} // namespace bp
