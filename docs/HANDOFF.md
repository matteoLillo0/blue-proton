# HANDOFF – pipeline edge C++

Pipeline che gira sulla **Arduino UNO Q** (lato Linux):
**camera/video → detector YOLO → tracker → conteggio → status.json**.
Tutti i pezzi sono veri. Ogni pezzo sta dietro **una** interfaccia e si sostituisce senza
toccare il resto. Dati e regole: `docs/contract.md`.

## Preparare, compilare, lanciare

Serve CMake ≥ 3.16, un compilatore C++17 e **OpenCV 4.x o 5.x** (moduli core, imgproc,
videoio, dnn). Debian / UNO Q: `sudo apt install libopencv-dev`. Arch: `sudo pacman -S opencv`.

```sh
tools/export_model.sh            # scarica YOLO11n (COCO) e lo esporta in models/yolo11n_640.onnx
tools/export_model.sh yolo11n 320   # variante 320 px, ~4x più veloce
tools/get_test_videos.sh         # video veri di bovini (licenza libera) in data/videos/
cmake -S edge -B edge/build
cmake --build edge/build -j
```

Modelli e video **non** sono in git: si rigenerano con gli script (servono `uv` e `curl`).

```sh
# Camera (V4L2): Ctrl+C per uscire
./edge/build/blue_proton_edge --camera /dev/video0
./edge/build/blue_proton_edge --camera /dev/video0 --width 1280 --height 720 --fps 30

# File video: alla velocità del video (come una camera) o il più veloce possibile (benchmark)
./edge/build/blue_proton_edge --video data/videos/jersey.webm
./edge/build/blue_proton_edge --video data/videos/jersey.webm --no-pace

# Modello più leggero, e video di debug con box e id disegnati
./edge/build/blue_proton_edge --video data/videos/jersey.webm \
    --model models/yolo11n_320.onnx --model-size 320 --debug-video /tmp/debug.mp4
```

Lanciato senza opzioni (o con un'opzione sbagliata) mostra l'elenco completo. Ogni secondo
stampa `fps`, `count` e `unique` e aggiorna `status.json`. All'uscita stampa i totali,
compresi i **ms per frame del detector**: è il numero da misurare sulla scheda.

## I pezzi

| Pezzo | Interfaccia | Implementazione |
|---|---|---|
| Camera | `FrameSource` | `V4l2CameraSource` (`--camera`) |
| File video / URL | `FrameSource` | `OpenCvVideoSource` (`--video`) |
| Detector | `IDetector` | `OpenCvYoloDetector` (`--model`) |
| Tracker | `ITracker` | `IouTracker` |
| Uscita | `StatusWriter` | `JsonStatusWriter` |

Per sostituire un pezzo (es. detector su un altro runtime): nuova classe che eredita
dall'interfaccia, `.cpp` in `edge/CMakeLists.txt`, e in `main.cpp` si cambia solo la riga
`std::make_unique<...>` di quel pezzo.

**Camera.** V4L2 diretto, formato **YUYV** convertito in BGR da noi. Se il detector è più
lento della camera, i frame arretrati vengono scartati: si analizza sempre il più recente.
Risoluzione e fps sono richieste: la camera sceglie i valori supportati più vicini e il
programma stampa quelli effettivi (`v4l2-ctl -d /dev/video0 --list-formats-ext` per vederli).

**Detector.** YOLOv8/YOLO11 in ONNX eseguito con OpenCV DNN su CPU. Letterbox → RGB 0..1
NCHW → inferenza → decodifica → NMS → box riportate alle coordinate normalizzate del frame
originale (togliendo il padding). La parte dopo l'inferenza (`yolo_postprocess.hpp`) è C++
puro, indipendente da OpenCV: si riusa identica con un altro runtime.
Oggi usa il modello **pubblico COCO**, classe 19 = cow. Con il nostro modello:
`--model nostro.onnx --class-id 0`, purché sia esportato da ultralytics come YOLOv8/11.

**Tracker.** `IouTracker`: abbina le box tra frame per sovrapposizione (IoU) con previsione
a velocità costante (stile SORT senza Kalman). Una traccia riceve un id dopo `min_hits`
frame consecutivi (filtra i falsi positivi); resta aperta `max_lost_s` = 2 s senza
detection, ma solo 0,4 s se è sul bordo (l'animale è uscito). Le durate sono in **secondi**,
quindi valgono uguali a 30 fps su PC e a pochi fps sulla scheda.
`main.cpp` conta gli id distinti → `unique_count` in `status.json`.

## Test

```sh
cmake --build edge/build && ctest --test-dir edge/build --output-on-failure
```

- `unit`: tracker (id stabili, conferma, occlusioni, incroci, uscita e rientro dallo stesso
  lato, tolleranza uguale a 3 e 30 fps), postprocessing YOLO (letterbox, riconversione box,
  padding, NMS), e pipeline tracker + detector **simulato** su 200 s a 3, 10 e 25 fps.
  Il detector simulato (`edge/tests/fake_detector.*`) esiste solo nei test.
- `real_video`: modello vero sul video della mandria; fallisce se non trova nessuna mucca.
  Si attiva solo se modello e video sono stati scaricati.

## Risultati sui video di prova (PC x86, 8 core)

| Video | Detector 640 | Detector 320 | Animali unici |
|---|---|---|---|
| `jersey.webm`, mandria di giorno | ~75 ms/frame | ~18 ms/frame | 23 (vedi sotto) |
| `cow_grid.webm`, una mucca di notte | ~80 ms/frame | – | 2 (reale: 1) |

- Di giorno le box sono precise. Il conteggio di `jersey` non è significativo: il video è
  girato a mano camminando, l'inquadratura cambia di continuo. La nostra camera sarà fissa.
- Di notte la mucca, girata verso i fari, non viene riconosciuta per ~5 s e riceve un id
  nuovo. È un limite del modello COCO generico; abbassare `--conf` peggiora (4 animali).
- Sulla UNO Q (4 core Cortex-A53) la CPU è molto più lenta: stima 5–15 volte, **da misurare**.
  Probabilmente servirà il modello 320.

## Punti aperti

- **Misure sulla UNO Q**: ms/frame del detector (640 vs 320), memoria, temperatura.
  OpenCV di Debian è la 4.x: il codice usa solo API comuni a 4 e 5, da verificare compilando lì.
- **Modello nostro**: addestrato su riprese della nostra camera, anche notturne. Poi
  `--class-id 0` e soglie da ritarare con `--debug-video`.
- **Camera finale**: se dà solo MJPEG/H.264 serve un decoder (si può usare `--video` con
  una pipeline GStreamer o un URL, oppure aggiungere il supporto MJPEG alla sorgente V4L2).
- **Conteggio**: oggi `unique_count` = animali diversi dall'avvio, riparte da 0 al riavvio.
  Da decidere se serve per finestra di tempo, per direzione (ingressi/uscite) o persistente.
  Con animali ammassati il tracker semplice può scambiare id: valutare ByteTrack su riprese vere.
- **Lato microcontrollore della UNO Q** (STM32): non ancora usato. Se deve mostrare il
  conteggio o comandare qualcosa, va aggiunto un `StatusWriter` che gli parli.
