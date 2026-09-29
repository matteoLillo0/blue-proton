// Pipeline edge di Blue Proton: sorgente -> detector -> tracker -> conteggio -> status.json.
// Oggi tutti i pezzi sono FINTI; per innestare quelli veri vedi docs/HANDOFF.md.

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>

#include "bp/fake_detector.hpp"
#include "bp/fake_frame_source.hpp"
#include "bp/json_status_writer.hpp"
#include "bp/null_tracker.hpp"
#include "bp/v4l2_camera_source.hpp"

namespace {

// Un signal handler puo' toccare in sicurezza solo variabili atomiche lock-free:
// si limita ad alzare questo flag, il loop principale lo controlla e si chiude.
std::atomic<bool> g_stop{false};
static_assert(std::atomic<bool>::is_always_lock_free, "serve un atomic lock-free per il signal handler");

void on_signal(int /*signum*/) { g_stop = true; }

struct Options {
    std::string status_file = "status.json";
    double max_seconds = 0.0;  // 0 = senza limite
    std::string camera;        // vuoto = sorgente finta
    int width = 640;
    int height = 480;
    double fps = 10.0;
    std::string save_frame;    // se impostato, salva il primo frame come immagine PPM
};

void print_usage(const char* prog) {
    std::cout << "Uso: " << prog << " [opzioni]\n"
              << "  --status-file <path>   dove scrivere status.json (default: status.json)\n"
              << "  --max-seconds <s>      esce dopo <s> secondi (default: mai)\n"
              << "  --camera <device>      usa una camera V4L2, es. /dev/video0 (default: sorgente finta)\n"
              << "  --width <px> --height <px> --fps <n>   richiesta alla sorgente (default: 640x480 @ 10)\n"
              << "  --save-frame <file.ppm>                salva il primo frame per controllarlo a occhio\n";
}

// Ritorna false se gli argomenti non sono validi.
bool parse_args(int argc, char** argv, Options& opt) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const bool has_value = i + 1 < argc;
        if (arg == "--status-file" && has_value) {
            opt.status_file = argv[++i];
        } else if (arg == "--camera" && has_value) {
            opt.camera = argv[++i];
        } else if (arg == "--save-frame" && has_value) {
            opt.save_frame = argv[++i];
        } else if ((arg == "--max-seconds" || arg == "--width" || arg == "--height" || arg == "--fps") && has_value) {
            double value = 0.0;
            try {
                value = std::stod(argv[++i]);
            } catch (const std::exception&) {
                return false;
            }
            if (arg == "--max-seconds") {
                opt.max_seconds = value;
            } else if (value <= 0.0) {
                return false;
            } else if (arg == "--width") {
                opt.width = static_cast<int>(value);
            } else if (arg == "--height") {
                opt.height = static_cast<int>(value);
            } else {
                opt.fps = value;
            }
        } else {
            return false;
        }
    }
    return true;
}

// PPM (P6): il formato immagine piu' semplice che esista, si apre con qualunque visualizzatore.
// Vuole RGB, il Frame e' BGR: invertiamo i canali.
bool save_ppm(const bp::Frame& frame, const std::string& path) {
    std::ofstream f(path, std::ios::binary);
    f << "P6\n" << frame.width << ' ' << frame.height << "\n255\n";
    for (std::size_t i = 0; i + 2 < frame.data.size(); i += 3) {
        const char rgb[3] = {static_cast<char>(frame.data[i + 2]), static_cast<char>(frame.data[i + 1]),
                             static_cast<char>(frame.data[i])};
        f.write(rgb, 3);
    }
    return static_cast<bool>(f);
}

std::unique_ptr<bp::FrameSource> make_source(const Options& opt) {
    if (opt.camera.empty()) {
        return std::make_unique<bp::FakeFrameSource>(opt.fps, opt.width, opt.height);
    }
    auto cam = std::make_unique<bp::V4l2CameraSource>(opt.camera, opt.width, opt.height, opt.fps);
    std::cout << "Camera " << opt.camera << ": " << cam->width() << 'x' << cam->height() << " @ " << cam->fps()
              << " fps\n";
    return cam;
}

double unix_now_s() {
    // system_clock (non steady_clock) perche' serve l'ora reale per il server.
    return std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
}

} // namespace

int main(int argc, char** argv) {
    Options opt;
    if (!parse_args(argc, argv, opt)) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    std::signal(SIGINT, on_signal);   // Ctrl+C
    std::signal(SIGTERM, on_signal);  // kill / systemctl stop sulla scheda

    // Il main possiede ogni pezzo tramite unique_ptr all'INTERFACCIA:
    // per usare un pezzo vero si cambia solo la riga di costruzione.
    std::unique_ptr<bp::FrameSource> source;
    try {
        source = make_source(opt);
    } catch (const std::exception& e) {
        std::cerr << "Errore sorgente: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
    std::unique_ptr<bp::IDetector> detector = std::make_unique<bp::FakeDetector>();
    // TODO(team): sostituire con il detector vero (es. YOLO) quando il modello e' pronto.
    std::unique_ptr<bp::ITracker> tracker = std::make_unique<bp::NullTracker>();
    // TODO(team): sostituire con il tracker vero.
    std::unique_ptr<bp::StatusWriter> writer = std::make_unique<bp::JsonStatusWriter>(opt.status_file);

    using Clock = std::chrono::steady_clock;
    const auto start = Clock::now();
    auto window_start = start;
    int frames_in_window = 0;
    int last_count = 0;
    bp::Frame frame;  // fuori dal loop: il buffer dei pixel viene riusato

    std::cout << "Blue Proton edge avviato. status: " << opt.status_file << " (Ctrl+C per uscire)\n";

    while (!g_stop) {
        if (!source->read(frame)) {
            std::cout << "Sorgente terminata.\n";
            break;
        }
        if (!opt.save_frame.empty() && !frame.data.empty()) {
            std::cout << (save_ppm(frame, opt.save_frame) ? "Frame salvato in " : "ERRORE salvataggio frame in ")
                      << opt.save_frame << '\n';
            opt.save_frame.clear();  // solo il primo
        }
        const auto detections = detector->detect(frame);
        const auto tracks = tracker->update(detections);
        // Conteggio PROVVISORIO: oggetti nel frame corrente. Il conteggio vero
        // (animali unici) arrivera' con il tracker.
        last_count = static_cast<int>(tracks.size());
        ++frames_in_window;

        const auto now = Clock::now();
        const double window_s = std::chrono::duration<double>(now - window_start).count();
        if (window_s >= 1.0) {
            bp::Status status;
            status.timestamp = unix_now_s();
            status.fps = frames_in_window / window_s;
            status.count = last_count;
            const bool ok = writer->write(status);

            const double elapsed_s = std::chrono::duration<double>(now - start).count();
            std::cout << std::fixed << std::setprecision(1) << "[t=" << elapsed_s << "s] fps=" << status.fps
                      << " count=" << status.count << (ok ? "" : "  ERRORE scrittura status") << '\n';

            window_start = now;
            frames_in_window = 0;
            if (opt.max_seconds > 0.0 && elapsed_s >= opt.max_seconds) {
                break;
            }
        }
    }

    // Nessuna pulizia manuale: i unique_ptr distruggono tutto uscendo da main.
    std::cout << "Chiusura pulita.\n";
    return EXIT_SUCCESS;
}
