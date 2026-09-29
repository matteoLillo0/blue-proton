#include "bp/json_status_writer.hpp"

#include <fstream>
#include <locale>
#include <sstream>
#include <system_error>
#include <utility>

namespace bp {

JsonStatusWriter::JsonStatusWriter(std::filesystem::path path)
    : path_(std::move(path)), tmp_path_(path_.string() + ".tmp") {}

bool JsonStatusWriter::write(const Status& status) {
    // Serializzazione a mano: lo schema e' piccolo e fisso, una libreria JSON
    // sarebbe una dipendenza in piu' per 4 campi.
    std::ostringstream json;
    // Locale "C" forzato: con un locale italiano i decimali potrebbero uscire
    // con la virgola (10,0), che rende il JSON non valido.
    json.imbue(std::locale::classic());
    json.setf(std::ios::fixed);
    json << "{\n"
         << "  \"schema_version\": " << Status::kSchemaVersion << ",\n";
    json.precision(3);
    json << "  \"timestamp\": " << status.timestamp << ",\n";
    json.precision(2);
    json << "  \"fps\": " << status.fps << ",\n"
         << "  \"count\": " << status.count << ",\n"
         << "  \"unique_count\": " << status.unique_count << "\n"
         << "}\n";

    {
        // Scope dedicato: il file viene chiuso (RAII) prima del rename.
        std::ofstream file(tmp_path_, std::ios::trunc);
        if (!file) {
            return false;
        }
        file << json.str();
        file.flush();
        if (!file) {
            return false;  // disco pieno o errore di scrittura: non tocchiamo il file buono
        }
    }

    // rename sostituisce il file finale in un solo passo.
    std::error_code ec;
    std::filesystem::rename(tmp_path_, path_, ec);
    return !ec;
}

} // namespace bp
