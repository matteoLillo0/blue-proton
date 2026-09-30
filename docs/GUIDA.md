# Guida rapida a Blue Proton

## Che cosa fa

Il programma guarda un video o una camera, trova le mucche con l'intelligenza artificiale
(YOLO), dà a ogni animale un numero e conta:

- **quante mucche si vedono adesso** (`count`)
- **quante mucche diverse sono passate** da quando è partito (`unique`)

Ogni secondo scrive questi numeri nel terminale e nel file `status.json`.

Per i dettagli tecnici vedi `docs/HANDOFF.md` e `docs/contract.md`.

## 1. Preparazione (una volta sola)

Installa gli strumenti:

```sh
# Sul PC (Arch)
sudo pacman -S cmake opencv curl uv

# Sulla UNO Q (Debian)
sudo apt install cmake g++ libopencv-dev curl
```

Poi, dalla cartella del progetto:

```sh
tools/export_model.sh               # scarica il modello YOLO (grande, 640)
tools/export_model.sh yolo11n 320   # scarica il modello piccolo e veloce (320)
tools/get_test_videos.sh            # scarica due video di mucche per le prove
```

Questi comandi creano le cartelle `models/` e `data/videos/`, che non sono in git.

## 2. Compilare

```sh
cmake -S edge -B edge/build
cmake --build edge/build -j
```

Il programma compilato è `edge/build/blue_proton_edge`. Ripeti questi due comandi ogni volta
che modifichi il codice.

## 3. Usarlo

**Prova con un video:**

```sh
./edge/build/blue_proton_edge --video data/videos/jersey.webm
```

**Con una camera collegata:**

```sh
./edge/build/blue_proton_edge --camera /dev/video0
```

Per fermarlo premi **Ctrl+C**. Mentre gira vedi una riga al secondo:

```
[t=5.0s] fps=10.0 count=3 unique=7
```

- `fps`: quanti fotogrammi analizza al secondo
- `count`: mucche visibili adesso
- `unique`: mucche diverse viste finora

Quando si chiude stampa anche i **ms per frame del detector**, cioè quanto tempo impiega l'IA
su ogni fotogramma. È il numero da misurare sulla scheda.

## 4. Vedere cosa ha riconosciuto

Per ottenere un video con i riquadri e i numeri disegnati sopra gli animali:

```sh
./edge/build/blue_proton_edge --video data/videos/jersey.webm --debug-video /tmp/prova.mp4
```

Poi apri `/tmp/prova.mp4` con un qualsiasi lettore video.

## 5. Opzioni utili

| Opzione | A cosa serve |
|---|---|
| `--model models/yolo11n_320.onnx --model-size 320` | modello più veloce (consigliato sulla UNO Q) |
| `--conf 0.5` | più severo, meno errori ma rischia di perdere qualche mucca (default 0.35) |
| `--width 1280 --height 720 --fps 30` | risoluzione e velocità chieste alla camera |
| `--max-seconds 60` | si ferma da solo dopo 60 secondi |
| `--no-pace` | analizza il video il più veloce possibile, per misurare le prestazioni |
| `--status-file /percorso/status.json` | dove salvare il file dei conteggi |

Se lanci il programma senza opzioni, stampa l'elenco completo.

⚠️ `--model-size` deve essere **lo stesso numero** usato per creare il modello: 320 con
`_320.onnx`, 640 con `_640.onnx`.

## 6. Con il vostro modello (quando sarà pronto)

```sh
./edge/build/blue_proton_edge --camera /dev/video0 \
    --model models/nostro.onnx --model-size 640 --class-id 0
```

Il modello deve essere un YOLOv8 o YOLO11 esportato in ONNX da ultralytics.

⚠️ **Non dimenticare `--class-id 0`.** Senza, il programma cerca la classe 19 (la "mucca" del
modello pubblico COCO), non trova nulla e il conteggio resta a 0 **senza dare errori**.

## 7. Controllare che funzioni tutto

```sh
ctest --test-dir edge/build --output-on-failure
```

Se alla fine leggi `100% tests passed`, è tutto a posto.

## Problemi comuni

| Messaggio | Soluzione |
|---|---|
| `modello non trovato` | lancia `tools/export_model.sh` |
| `--model-size corrisponde al modello?` | il numero di `--model-size` non è quello del modello |
| `la camera non supporta YUYV` | la camera dà solo altri formati (es. MJPEG), non ancora supportati: controlla con `v4l2-ctl -d /dev/video0 --list-formats-ext` |
| `impossibile aprire la camera` | la camera non è collegata, oppure è su un altro `/dev/videoN` (lancia `ls /dev/video*`) |
| `count` sempre 0 | controlla `--class-id`, oppure prova ad abbassare `--conf` |
