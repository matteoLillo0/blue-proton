# HANDOFF – scheletro edge C++

Scheletro della pipeline edge: **sorgente → detector → tracker → conteggio → status.json**.
Già veri: camera, file video, tracker e conteggio. Ancora finto: il detector. Ogni pezzo si sostituisce
implementando **una** interfaccia, senza toccare il resto. Dati e regole: `docs/contract.md`.

## Compilare e lanciare

Serve solo CMake ≥ 3.16 e un compilatore C++17. **OpenCV è opzionale** (4.x o 5.x, moduli
core/imgproc/videoio): se CMake la trova si abilita la lettura da file video (`--video`),
altrimenti tutto il resto compila e funziona uguale. Per escluderla: `-DBP_WITH_OPENCV=OFF`.

```sh
cmake -S edge -B edge/build
cmake --build edge/build -j
./edge/build/blue_proton_edge                      # gira finché non premi Ctrl+C
./edge/build/blue_proton_edge --max-seconds 10 --status-file /tmp/status.json
```

Ogni secondo stampa `fps` e `count` e aggiorna `status.json` (default: cartella corrente).

**Camera vera (Linux, V4L2).** Senza `--camera` si usa la sorgente finta.

```sh
./edge/build/blue_proton_edge --camera /dev/video0                       # 640x480 @ 10 fps
./edge/build/blue_proton_edge --camera /dev/video0 --width 1280 --height 720 --fps 30
./edge/build/blue_proton_edge --camera /dev/video0 --save-frame /tmp/f.ppm   # salva il 1° frame
```

Risoluzione e fps sono *richieste*: la camera sceglie i valori supportati più vicini e il
programma stampa quelli effettivi. Formati e risoluzioni: `v4l2-ctl -d /dev/video0 --list-formats-ext`.
Con poca luce molte webcam abbassano da sole gli fps (esposizione più lunga).

**File video (richiede OpenCV).**

```sh
./edge/build/blue_proton_edge --video mucche.mp4             # alla velocità del video, come una camera
./edge/build/blue_proton_edge --video mucche.mp4 --no-pace   # il più veloce possibile: benchmark
```

A fine video il programma esce e stampa frame totali e fps medi. `timestamp_s` è il tempo
*nel video* (frame / fps), così il tracker vede gli intervalli veri anche con `--no-pace`.

## Dove si innesta ogni pezzo

| Pezzo vero | Interfaccia da implementare | Pezzo finto da sostituire |
|---|---|---|
| Camera | `FrameSource` (`frame_source.hpp`) | ✅ `V4l2CameraSource` (`--camera`) |
| File video | `FrameSource` (`frame_source.hpp`) | ✅ `OpenCvVideoSource` (`--video`) |
| Modello YOLO | `IDetector` (`detector.hpp`) | `FakeDetector` |
| Tracker | `ITracker` (`tracker.hpp`) | ✅ `IouTracker` |
| Invio al server | `StatusWriter` (`status_writer.hpp`) | `JsonStatusWriter` (resta valido) |

Procedura, uguale per tutti:

1. Crea `edge/include/bp/<nome>.hpp` e `edge/src/<nome>.cpp` con una classe che eredita
   dall'interfaccia (guarda la versione finta come esempio).
2. Aggiungi il `.cpp` a `add_library(bp_edge ...)` in `edge/CMakeLists.txt`.
3. In `edge/src/main.cpp` cambia **solo** la riga `std::make_unique<...>` del pezzo.

**Modello YOLO.** `detect()` riceve un `Frame` BGR. Deve fare il preprocessing
(resize/letterbox, BGR→RGB, normalizzazione) e l'inferenza. Poi deve **riconvertire** le box
dallo spazio del modello alle coordinate normalizzate del frame originale, togliendo il padding.

**Tracker.** `IouTracker` (`iou_tracker.hpp`): abbina le box tra frame per sovrapposizione
(IoU) con previsione a velocità costante, stile SORT senza Kalman. Una traccia riceve un id
solo dopo `min_hits` frame consecutivi (filtra i falsi positivi); resta aperta per `max_misses`
frame senza detection, ma solo `max_misses_at_edge` se è sul bordo (l'animale è uscito).
I parametri sono in **frame**, non secondi: i default sono pensati per 10–25 fps.
`main.cpp` conta gli id distinti → `unique_count` in `status.json`.
Il vecchio `NullTracker` resta come esempio minimo ma non va più usato (gonfierebbe `unique_count`).

**Detector finto.** Simula animali che attraversano la scena ogni 4 s, con detection
mancate, tremolio e falsi positivi, in modo deterministico: serve a provare il tracker
finché non c'è il modello.

## Test

```sh
cmake --build edge/build && ctest --test-dir edge/build --output-on-failure
```

`edge/tests/test_tracker.cpp`: tracker (id stabili, conferma, occlusioni, incroci, uscita e
rientro dallo stesso lato) e pipeline detector finto + tracker su 200 s a 10 e 25 fps.

## Punti aperti

- Sorgente camera: fatta con V4L2 diretto, **solo formato YUYV** (convertito in BGR da noi,
  nessuna dipendenza). Le camere che danno solo MJPEG/H.264 (alcune USB, molte IP) richiedono
  un decoder: libjpeg-turbo, OpenCV o GStreamer. Le camere CSI delle schede (Jetson, Raspberry)
  di solito passano da GStreamer/libcamera: da verificare sulla scheda finale.
- OpenCV è entrata come dipendenza opzionale (solo per `--video`). Se il detector la usa per
  resize/letterbox diventa obbligatoria: in quel caso togliere l'opzione in `CMakeLists.txt`.
  `--video` accetta anche URL (es. `rtsp://`) via `cv::VideoCapture`: utile per camere IP, non testato.
- Modello: dimensione d'ingresso, formato del tensore (NHWC/NCHW, float/uint8), soglie NMS.
- Conteggio: oggi `unique_count` = animali diversi dall'avvio. Da decidere se serve un
  conteggio per finestra di tempo, per direzione (ingressi/uscite da un cancello), o che
  sopravviva ai riavvii. Da verificare su video veri: soglie del tracker, scambi di id con
  animali ammassati (lì servirebbe Kalman + algoritmo ungherese o ByteTrack).
- Misure di FPS e memoria sulla scheda finale.
