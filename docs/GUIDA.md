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

Se la camera dà errore `la camera non supporta YUYV`, oppure va lenta ad alta risoluzione,
usa `--video` al posto di `--camera` (legge anche il formato MJPEG delle webcam):

```sh
./edge/build/blue_proton_edge --video /dev/video0 --width 1280 --height 720 --fps 30
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

Il modello deve essere un YOLOv8 o YOLO11 addestrato con ultralytics. Dall'addestramento
esce un file di pesi, di solito `runs/detect/train/weights/best.pt`. Convertilo così:

```sh
tools/export_model.sh percorso/best.pt 320   # crea models/best_320.onnx
```

Poi lancialo:

```sh
./edge/build/blue_proton_edge --camera /dev/video0 \
    --model models/best_320.onnx --model-size 320 --class-id 0
```

`--class-id 0` serve se il modello conosce una sola classe (la mucca). Se lo dimentichi, il
programma si ferma con un errore che te lo ricorda.

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
| `la camera non supporta YUYV` | usa `--video /dev/video0` al posto di `--camera` (formati della camera: `v4l2-ctl -d /dev/video0 --list-formats-ext`) |
| `--class-id N non esiste` | il modello non ha quella classe: con il vostro modello usa `--class-id 0` |
| `impossibile aprire la camera` | la camera non è collegata, oppure è su un altro `/dev/videoN` (lancia `ls /dev/video*`) |
| `count` sempre 0 | prova ad abbassare `--conf`, e guarda il `--debug-video` |
