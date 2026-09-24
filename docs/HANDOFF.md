# HANDOFF – scheletro edge C++

Scheletro della pipeline edge: **sorgente → detector → tracker → conteggio → status.json**.
Oggi tutti i pezzi sono finti ma il programma gira. Ognuno di voi sostituisce un pezzo
implementando **una** interfaccia, senza toccare il resto. Dati e regole: `docs/contract.md`.

## Compilare e lanciare

Serve solo CMake ≥ 3.16 e un compilatore C++17. Nessuna libreria esterna.

```sh
cmake -S edge -B edge/build
cmake --build edge/build -j
./edge/build/blue_proton_edge                      # gira finché non premi Ctrl+C
./edge/build/blue_proton_edge --max-seconds 10 --status-file /tmp/status.json
```

Ogni secondo stampa `fps` e `count` e aggiorna `status.json` (default: cartella corrente).

## Dove si innesta ogni pezzo

| Pezzo vero | Interfaccia da implementare | Pezzo finto da sostituire |
|---|---|---|
| Camera / file video | `FrameSource` (`frame_source.hpp`) | `FakeFrameSource` |
| Modello YOLO | `IDetector` (`detector.hpp`) | `FakeDetector` |
| Tracker | `ITracker` (`tracker.hpp`) | `NullTracker` |
| Invio al server | `StatusWriter` (`status_writer.hpp`) | `JsonStatusWriter` (resta valido) |

Procedura, uguale per tutti:

1. Crea `edge/include/bp/<nome>.hpp` e `edge/src/<nome>.cpp` con una classe che eredita
   dall'interfaccia (guarda la versione finta come esempio).
2. Aggiungi il `.cpp` a `add_library(bp_edge ...)` in `edge/CMakeLists.txt`.
3. In `edge/src/main.cpp` cambia **solo** la riga `std::make_unique<...>` del pezzo.

**Modello YOLO.** `detect()` riceve un `Frame` BGR. Deve fare il preprocessing
(resize/letterbox, BGR→RGB, normalizzazione) e l'inferenza. Poi deve **riconvertire** le box
dallo spazio del modello alle coordinate normalizzate del frame originale, togliendo il padding.

**Tracker.** Deve rendere `track_id` stabile tra frame. Il conteggio in `main.cpp` oggi
conta le box del frame. Col tracker vero andrà contato il numero di animali unici.

## Punti aperti

- Sorgente vera: con OpenCV (`cv::VideoCapture`) o altro? È l'unica dipendenza da decidere.
- Modello: dimensione d'ingresso, formato del tensore (NHWC/NCHW, float/uint8), soglie NMS.
- Conteggio definitivo e campi aggiuntivi di `status.json` (incrementare `schema_version`).
- Misure di FPS e memoria sulla scheda finale.
