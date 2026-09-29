# Contratto dati – Blue Proton edge

Definizione in codice: `edge/include/bp/types.hpp`. Ogni modifica va concordata con il team.

## Tipi

```cpp
struct Frame {
    int width, height;               // pixel
    std::vector<uint8_t> data;       // BGR, 3 byte/pixel, righe contigue: size = width*height*3
    double timestamp_s;              // secondi dall'inizio della sorgente, monotono (video: tempo nel video)
};

struct Detection {
    float x, y, w, h;                // NORMALIZZATE 0..1 (vedi regole)
    int   class_id;                  // 0 = bovino (per ora unica classe)
    float confidence;                // 0..1
};

struct TrackedDetection {
    Detection det;
    int track_id;                    // assegnato dal tracker, -1 = nessuno
};
```

## Regole

1. **Coordinate normalizzate.** `x, y, w, h` sono in 0..1 rispetto al frame **originale**.
   `x, y` è l'angolo **in alto a sinistra**, `w, h` sono larghezza e altezza.
   Per tornare ai pixel: `px = x * frame.width`, `py = y * frame.height`.
2. **Perché normalizzate.** Tracker, server e dashboard non devono sapere la risoluzione
   della camera né quella d'ingresso del modello (es. 640x640 con letterbox).
   La riconversione dallo spazio del modello allo spazio del frame è compito del detector.
3. **Box nel frame.** Il detector restituisce box già ritagliate dentro 0..1.
4. **Dimensione dei dati.** Chi usa `data` controlla `data.size() == width*height*3`
   prima di usarlo (un detector che riceve un frame malformato restituisce zero box).
5. **Un frame alla volta.** `ITracker::update(detections, frame.timestamp_s)` va chiamato una
   volta per frame, in ordine, con il timestamp del frame da cui vengono le detection:
   il tracker ragiona in secondi, perché gli fps cambiano tra PC, scheda e video.
6. **Id di traccia.** `update()` restituisce una `TrackedDetection` per detection, nello stesso
   ordine. `track_id` è `-1` finché la traccia non è confermata; gli id confermati non vengono
   mai riusati (il conteggio degli animali unici si basa su questo).

## status.json (`schema_version` 2)

```json
{ "schema_version": 2, "timestamp": 1790265681.894, "fps": 10.00, "count": 3, "unique_count": 17 }
```

| Campo | Tipo | Significato |
|---|---|---|
| `schema_version` | int | Aumenta se cambia il significato di un campo. |
| `timestamp` | float | Secondi Unix UTC al momento della scrittura. |
| `fps` | float | Frame elaborati al secondo nell'ultimo secondo. |
| `count` | int | Animali con traccia **confermata** nell'ultimo frame (visibili adesso). |
| `unique_count` | int | Animali diversi visti dall'avvio del programma (id di traccia distinti). Riparte da 0 a ogni riavvio. |

Da v1 a v2: `count` non conta più tutte le box ma solo le tracce confermate
(i falsi positivi di un frame non entrano), ed è stato aggiunto `unique_count`.

Il file viene aggiornato una volta al secondo con scrittura atomica
(`status.json.tmp` + rename): chi lo legge non vede mai un file a metà.
