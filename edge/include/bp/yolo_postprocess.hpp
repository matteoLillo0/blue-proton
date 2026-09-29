#pragma once

// Parti del detector YOLO che NON dipendono dal runtime di inferenza (OpenCV DNN oggi,
// magari ONNX Runtime o altro sulla UNO Q domani): geometria del letterbox, decodifica
// dell'output, NMS. C++ puro, quindi testabile senza modello.

#include <vector>

#include "bp/types.hpp"

namespace bp {

// Letterbox: il frame viene ridimensionato mantenendo le proporzioni per stare in un
// quadrato input_size x input_size, e il resto viene riempito di grigio (padding).
// Esempio: 1280x720 -> 640x360 (scale 0.5), con 140 px di padding sopra e sotto.
struct Letterbox {
    float scale = 1.0F;  // pixel del modello per pixel del frame
    int new_w = 0;       // dimensioni del frame ridimensionato, senza padding
    int new_h = 0;
    int pad_x = 0;       // padding a sinistra (e circa uguale a destra)
    int pad_y = 0;       // padding sopra (e circa uguale sotto)
    int input_size = 0;
};

Letterbox compute_letterbox(int frame_w, int frame_h, int input_size);

struct YoloDecodeParams {
    // Classe del MODELLO da tenere (COCO: 19 = cow). In uscita diventa class_id 0 (bovino),
    // come da contratto. Con il nostro modello a classe unica sara' 0.
    int model_class_id = 19;
    float conf_threshold = 0.35F;  // sotto questa confidenza la box viene scartata
    float nms_iou = 0.45F;         // box piu' sovrapposte di cosi' sono doppioni
};

// Decodifica l'output di YOLOv8/YOLO11 (ultralytics) e lo riporta al frame originale.
// Layout atteso: [channels][anchors] con channels = 4 + numero classi; per ogni anchor
// cx, cy, w, h in pixel del modello, poi uno score per classe (gia' tra 0 e 1).
// Restituisce box NORMALIZZATE 0..1 sul frame, ritagliate, dopo NMS, ordinate per confidenza.
std::vector<Detection> decode_yolo(const float* output, int channels, int anchors, const YoloDecodeParams& params,
                                   const Letterbox& lb, int frame_w, int frame_h);

// Non-Maximum Suppression: tra box che si sovrappongono oltre iou_threshold tiene quella
// con confidenza maggiore. Restituisce le box sopravvissute ordinate per confidenza.
std::vector<Detection> nms(std::vector<Detection> boxes, float iou_threshold);

// Numero di anchor che YOLOv8/11 produce per un certo input (griglie con stride 8, 16, 32).
// Serve a verificare che --model-size corrisponda davvero al modello caricato.
int yolo_expected_anchors(int input_size);

} // namespace bp
