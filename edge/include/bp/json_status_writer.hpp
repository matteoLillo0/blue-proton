#pragma once

#include <filesystem>

#include "bp/status_writer.hpp"

namespace bp {

// Scrive lo stato in un file JSON in modo ATOMICO: prima su "<path>.tmp",
// poi rename sul file finale. Chi legge (server, dashboard) vede sempre
// il file vecchio completo oppure quello nuovo completo, mai uno a meta'.
class JsonStatusWriter final : public StatusWriter {
public:
    explicit JsonStatusWriter(std::filesystem::path path);
    bool write(const Status& status) override;

private:
    std::filesystem::path path_;
    std::filesystem::path tmp_path_;  // stessa cartella: rename e' atomico solo sullo stesso filesystem
};

} // namespace bp
