#include "bp/yolo_postprocess.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "bp/geometry.hpp"

namespace bp {

Letterbox compute_letterbox(int frame_w, int frame_h, int input_size) {
    Letterbox lb;
    lb.input_size = input_size;
    lb.scale = std::min(static_cast<float>(input_size) / static_cast<float>(frame_w),
                        static_cast<float>(input_size) / static_cast<float>(frame_h));
    lb.new_w = std::max(1, static_cast<int>(std::lround(static_cast<float>(frame_w) * lb.scale)));
    lb.new_h = std::max(1, static_cast<int>(std::lround(static_cast<float>(frame_h) * lb.scale)));
    // Immagine centrata: stesso padding sui due lati (il pixel dispari va a destra/sotto).
    lb.pad_x = (input_size - lb.new_w) / 2;
    lb.pad_y = (input_size - lb.new_h) / 2;
    return lb;
}

int yolo_expected_anchors(int input_size) {
    int total = 0;
    for (const int stride : {8, 16, 32}) {
        const int cells = input_size / stride;
        total += cells * cells;
    }
    return total;
}

std::vector<Detection> nms(std::vector<Detection> boxes, float iou_threshold) {
    std::sort(boxes.begin(), boxes.end(),
              [](const Detection& a, const Detection& b) { return a.confidence > b.confidence; });
    std::vector<Detection> kept;
    for (const Detection& b : boxes) {
        const bool duplicate = std::any_of(kept.begin(), kept.end(),
                                           [&](const Detection& k) { return iou(k, b) > iou_threshold; });
        if (!duplicate) {
            kept.push_back(b);
        }
    }
    return kept;
}

std::vector<Detection> decode_yolo(const float* output, int channels, int anchors, const YoloDecodeParams& params,
                                   const Letterbox& lb, int frame_w, int frame_h) {
    std::vector<Detection> candidates;
    if (params.model_class_id < 0 || 4 + params.model_class_id >= channels) {
        return candidates;  // la classe richiesta non esiste in questo modello
    }
    const auto n = static_cast<std::size_t>(anchors);
    const float* cx = output;
    const float* cy = output + n;
    const float* w = output + 2 * n;
    const float* h = output + 3 * n;
    const float* score = output + (4 + static_cast<std::size_t>(params.model_class_id)) * n;
    const auto fw = static_cast<float>(frame_w);
    const auto fh = static_cast<float>(frame_h);

    for (std::size_t i = 0; i < n; ++i) {
        if (score[i] < params.conf_threshold) {
            continue;  // quasi tutti gli anchor finiscono qui: controllo per primo, e' il piu' economico
        }
        // Spazio del modello (pixel, centro) -> spazio del frame (pixel, angolo) -> normalizzato.
        // Si toglie il padding e si divide per la scala: l'inverso esatto del letterbox.
        const float x0 = (cx[i] - w[i] / 2.0F - static_cast<float>(lb.pad_x)) / lb.scale;
        const float y0 = (cy[i] - h[i] / 2.0F - static_cast<float>(lb.pad_y)) / lb.scale;
        const float x1 = (cx[i] + w[i] / 2.0F - static_cast<float>(lb.pad_x)) / lb.scale;
        const float y1 = (cy[i] + h[i] / 2.0F - static_cast<float>(lb.pad_y)) / lb.scale;
        // Contratto: box ritagliate dentro il frame.
        const float nx0 = std::clamp(x0 / fw, 0.0F, 1.0F);
        const float ny0 = std::clamp(y0 / fh, 0.0F, 1.0F);
        const float nx1 = std::clamp(x1 / fw, 0.0F, 1.0F);
        const float ny1 = std::clamp(y1 / fh, 0.0F, 1.0F);
        if (nx1 <= nx0 || ny1 <= ny0) {
            continue;  // box tutta nel padding: non corrisponde a niente nel frame
        }
        candidates.push_back(Detection{nx0, ny0, nx1 - nx0, ny1 - ny0, 0, score[i]});
    }
    return nms(std::move(candidates), params.nms_iou);
}

} // namespace bp
