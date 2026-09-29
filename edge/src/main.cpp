// Pipeline edge di Blue Proton: sorgente -> detector -> tracker -> conteggio -> status.json.
// Tutti i pezzi sono veri: camera (V4L2) o file video, YOLO (OpenCV DNN), tracker IoU.
// Per sostituirne uno vedi docs/HANDOFF.md.

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_set>

#include "bp/annotated_video_writer.hpp"
#include "bp/iou_tracker.hpp"
#include "bp/json_status_writer.hpp"
#include "bp/opencv_video_source.hpp"
#include "bp/opencv_yolo_detector.hpp"
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
    // Sorgente: esattamente una tra camera e video.
    std::string camera;
    std::string video;
    bool pace = true;          // video alla sua velocita' reale
    int width = 640;
    int height = 480;
    double fps = 10.0;
    // Detector.
    std::string model = "models/yolo11n_640.onnx";
    int model_size = 640;
    bp::YoloDecodeParams yolo;
    // Debug.
    std::string save_frame;    // salva il primo frame come immagine PPM
    std::string debug_video;   // salva un video con box e id disegnati
};

void print_usage(const char* prog) {
    std::cout << "Uso: " << prog << " (--camera <device> | --video <file>) [opzioni]\n"
              << "Sorgente:\n"
              << "  --camera <device>      camera V4L2, es. /dev/video0\n"
              << "  --width <px> --height <px> --fps <n>   richiesta alla camera (default: 640x480 @ 10)\n"
              << "  --video <file>         file video (o URL rtsp://)\n"
              << "  --no-pace              con --video: il piu' veloce possibile invece che a velocita' reale\n"
              << "Detector:\n"
              << "  --model <file.onnx>    modello YOLO (default: models/yolo11n_640.onnx)\n"
              << "  --model-size <px>      lato d'ingresso con cui e' stato esportato (default: 640)\n"
              << "  --class-id <n>         classe del modello da contare (default: 19 = cow in COCO)\n"
              << "  --conf <0..1>          confidenza minima (default: 0.35)\n"
              << "Uscita:\n"
              << "  --status-file <path>   dove scrivere status.json (default: status.json)\n"
              << "  --max-seconds <s>      esce dopo <s> secondi (default: mai)\n"
              << "  --save-frame <file.ppm>   salva il primo frame\n"
              << "  --debug-video <file.mp4>  salva il video con box e id disegnati\n";
}

// Ritorna false se gli argomenti non sono validi.
bool parse_args(int argc, char** argv, Options& opt) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const bool has_value = i + 1 < argc;
        if (arg == "--no-pace") {
            opt.pace = false;
            continue;
        }
        if (!has_value) {
            return false;
        }
        const std::string value = argv[++i];
        try {
            if (arg == "--status-file") {
                opt.status_file = value;
            } else if (arg == "--camera") {
                opt.camera = value;
            } else if (arg == "--video") {
                opt.video = value;
            } else if (arg == "--model") {
                opt.model = value;
            } else if (arg == "--save-frame") {
                opt.save_frame = value;
            } else if (arg == "--debug-video") {
                opt.debug_video = value;
            } else if (arg == "--max-seconds") {
                opt.max_seconds = std::stod(value);
            } else if (arg == "--width") {
                opt.width = std::stoi(value);
            } else if (arg == "--height") {
                opt.height = std::stoi(value);
            } else if (arg == "--fps") {
                opt.fps = std::stod(value);
            } else if (arg == "--model-size") {
                opt.model_size = std::stoi(value);
            } else if (arg == "--class-id") {
                opt.yolo.model_class_id = std::stoi(value);
            } else if (arg == "--conf") {
                opt.yolo.conf_threshold = std::stof(value);
            } else {
                return false;
            }
        } catch (const std::exception&) {
            return false;  // numero non valido
        }
    }
    const bool one_source = opt.camera.empty() != opt.video.empty();
    return one_source && opt.width > 0 && opt.height > 0 && opt.fps > 0.0 && opt.yolo.conf_threshold >= 0.0F &&
           opt.yolo.conf_threshold <= 1.0F;
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

// Crea la sorgente scelta e ne restituisce gli fps nominali in `fps_out`.
std::unique_ptr<bp::FrameSource> make_source(const Options& opt, double& fps_out) {
    if (!opt.video.empty()) {
        auto vid = std::make_unique<bp::OpenCvVideoSource>(opt.video, opt.pace);
        std::cout << "Video " << opt.video << ": " << vid->width() << 'x' << vid->height() << " @ " << vid->fps()
                  << " fps" << (opt.pace ? "" : " (senza pacing)") << '\n';
        fps_out = vid->fps();
        return vid;
    }
    auto cam = std::make_unique<bp::V4l2CameraSource>(opt.camera, opt.width, opt.height, opt.fps);
    std::cout << "Camera " << opt.camera << ": " << cam->width() << 'x' << cam->height() << " @ " << cam->fps()
              << " fps\n";
    fps_out = cam->fps();
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
    // per cambiare un pezzo si cambia solo la riga di costruzione.
    std::unique_ptr<bp::FrameSource> source;
    std::unique_ptr<bp::IDetector> detector;
    double source_fps = 0.0;
    try {
        source = make_source(opt, source_fps);
        detector = std::make_unique<bp::OpenCvYoloDetector>(opt.model, opt.model_size, opt.yolo);
        std::cout << "Modello " << opt.model << " (" << opt.model_size << " px, classe " << opt.yolo.model_class_id
                  << ", conf >= " << opt.yolo.conf_threshold << ")\n";
    } catch (const std::exception& e) {
        std::cerr << "Errore: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
    std::unique_ptr<bp::ITracker> tracker = std::make_unique<bp::IouTracker>();
    std::unique_ptr<bp::StatusWriter> writer = std::make_unique<bp::JsonStatusWriter>(opt.status_file);
    std::unique_ptr<bp::AnnotatedVideoWriter> debug_video;
    if (!opt.debug_video.empty()) {
        debug_video = std::make_unique<bp::AnnotatedVideoWriter>(opt.debug_video, source_fps);
    }

    using Clock = std::chrono::steady_clock;
    const auto start = Clock::now();
    auto window_start = start;
    int frames_in_window = 0;
    long frames_total = 0;
    double detect_s_total = 0.0;  // tempo speso nel detector: il collo di bottiglia sulla scheda
    int last_count = 0;
    // Id visti almeno una volta. Il tracker non riusa mai un id, quindi quanti id
    // diversi = quanti animali unici. Funziona con qualunque ITracker.
    std::unordered_set<int> seen_ids;
    bp::Frame frame;  // fuori dal loop: il buffer dei pixel viene riusato
    int exit_code = EXIT_SUCCESS;

    std::cout << "Blue Proton edge avviato. status: " << opt.status_file << " (Ctrl+C per uscire)\n";

    while (!g_stop) {
        if (!source->read(frame)) {
            std::cout << "Sorgente terminata.\n";
            break;
        }
        if (!opt.save_frame.empty()) {
            std::cout << (save_ppm(frame, opt.save_frame) ? "Frame salvato in " : "ERRORE salvataggio frame in ")
                      << opt.save_frame << '\n';
            opt.save_frame.clear();  // solo il primo
        }

        std::vector<bp::Detection> detections;
        const auto t0 = Clock::now();
        try {
            detections = detector->detect(frame);
        } catch (const std::exception& e) {
            std::cerr << "Errore detector: " << e.what() << '\n';
            exit_code = EXIT_FAILURE;
            break;
        }
        detect_s_total += std::chrono::duration<double>(Clock::now() - t0).count();

        const auto tracks = tracker->update(detections, frame.timestamp_s);
        // Contiamo solo le tracce confermate (id >= 0): le altre possono essere falsi positivi.
        last_count = 0;
        for (const auto& t : tracks) {
            if (t.track_id >= 0) {
                ++last_count;
                seen_ids.insert(t.track_id);
            }
        }
        if (debug_video && !debug_video->write(frame, tracks)) {
            std::cerr << "ERRORE scrittura " << opt.debug_video << ", video di debug disattivato\n";
            debug_video.reset();
        }
        ++frames_in_window;
        ++frames_total;

        const auto now = Clock::now();
        const double window_s = std::chrono::duration<double>(now - window_start).count();
        if (window_s >= 1.0) {
            bp::Status status;
            status.timestamp = unix_now_s();
            status.fps = frames_in_window / window_s;
            status.count = last_count;
            status.unique_count = static_cast<int>(seen_ids.size());
            const bool ok = writer->write(status);

            const double elapsed_s = std::chrono::duration<double>(now - start).count();
            std::cout << std::fixed << std::setprecision(1) << "[t=" << elapsed_s << "s] fps=" << status.fps
                      << " count=" << status.count << " unique=" << status.unique_count
                      << (ok ? "" : "  ERRORE scrittura status") << '\n';

            window_start = now;
            frames_in_window = 0;
            if (opt.max_seconds > 0.0 && elapsed_s >= opt.max_seconds) {
                break;
            }
        }
    }

    const double total_s = std::chrono::duration<double>(Clock::now() - start).count();
    std::cout << std::fixed << std::setprecision(1) << "Totale: " << frames_total << " frame in " << total_s
              << " s (fps medi " << (total_s > 0.0 ? frames_total / total_s : 0.0) << ", detector "
              << (frames_total > 0 ? 1000.0 * detect_s_total / frames_total : 0.0) << " ms/frame), animali unici "
              << seen_ids.size() << '\n';

    // Nessuna pulizia manuale: i unique_ptr distruggono tutto uscendo da main.
    std::cout << "Chiusura pulita.\n";
    return exit_code;
}
