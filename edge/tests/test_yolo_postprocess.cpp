// Test della parte del detector YOLO indipendente dal runtime: letterbox, decodifica, NMS.
// L'output del modello viene costruito a mano, quindi non serve il file .onnx.

#include <cmath>
#include <vector>

#include "bp/yolo_postprocess.hpp"
#include "check.hpp"

namespace {

bool near(float a, float b, float tol = 1e-4F) { return std::fabs(a - b) < tol; }

void test_letterbox() {
    // Frame 16:9 -> scala 0.5, 140 px di grigio sopra e sotto.
    const bp::Letterbox wide = bp::compute_letterbox(1280, 720, 640);
    CHECK(near(wide.scale, 0.5F));
    CHECK(wide.new_w == 640 && wide.new_h == 360);
    CHECK(wide.pad_x == 0 && wide.pad_y == 140);
    // Frame verticale (es. telefono): padding ai lati.
    const bp::Letterbox tall = bp::compute_letterbox(720, 1280, 640);
    CHECK(tall.new_w == 360 && tall.new_h == 640);
    CHECK(tall.pad_x == 140 && tall.pad_y == 0);
    // Frame gia' quadrato: nessun padding.
    const bp::Letterbox sq = bp::compute_letterbox(320, 320, 640);
    CHECK(near(sq.scale, 2.0F) && sq.pad_x == 0 && sq.pad_y == 0);
}

void test_expected_anchors() {
    CHECK(bp::yolo_expected_anchors(640) == 8400);  // 80*80 + 40*40 + 20*20
    CHECK(bp::yolo_expected_anchors(320) == 2100);
}

// Output finto nel layout di YOLOv8/11: [4 + 80 classi][anchors].
struct FakeOutput {
    static constexpr int kChannels = 4 + 80;
    int anchors;
    std::vector<float> data;
    explicit FakeOutput(int n) : anchors(n), data(static_cast<std::size_t>(kChannels) * n, 0.0F) {}
    float& at(int channel, int anchor) { return data[static_cast<std::size_t>(channel) * anchors + anchor]; }
    void set(int anchor, float cx, float cy, float w, float h, int cls, float score) {
        at(0, anchor) = cx;
        at(1, anchor) = cy;
        at(2, anchor) = w;
        at(3, anchor) = h;
        at(4 + cls, anchor) = score;
    }
};

void test_decode_maps_back_to_frame() {
    // Frame 1280x720, mucca a (100, 200) px, 300x150 px.
    // Nello spazio del modello (scala 0.5, pad_y 140): centro (125, 277.5), 150x75.
    const bp::Letterbox lb = bp::compute_letterbox(1280, 720, 640);
    FakeOutput out(3);
    out.set(0, 125.0F, 277.5F, 150.0F, 75.0F, 19, 0.9F);  // mucca
    out.set(1, 127.0F, 278.0F, 150.0F, 75.0F, 19, 0.6F);  // doppione della stessa mucca: via con NMS
    out.set(2, 400.0F, 300.0F, 50.0F, 100.0F, 0, 0.95F);  // persona: classe diversa, ignorata
    out.at(4 + 19, 2) = 0.1F;                             // ...e come mucca e' sotto soglia

    const bp::YoloDecodeParams params;  // classe 19, conf 0.35
    const auto dets = bp::decode_yolo(out.data.data(), FakeOutput::kChannels, out.anchors, params, lb, 1280, 720);
    CHECK(dets.size() == 1);
    if (dets.size() == 1) {
        CHECK(near(dets[0].x, 100.0F / 1280.0F));
        CHECK(near(dets[0].y, 200.0F / 720.0F));
        CHECK(near(dets[0].w, 300.0F / 1280.0F));
        CHECK(near(dets[0].h, 150.0F / 720.0F));
        CHECK(dets[0].class_id == 0);  // contratto: 0 = bovino
        CHECK(near(dets[0].confidence, 0.9F));
    }
}

void test_decode_clips_padding() {
    // Box che sborda nel padding in alto (y modello 50..150, il frame inizia a 140):
    // deve essere ritagliata al bordo del frame, non finire a y negativa.
    const bp::Letterbox lb = bp::compute_letterbox(1280, 720, 640);
    FakeOutput out(2);
    out.set(0, 320.0F, 100.0F, 100.0F, 100.0F, 19, 0.8F);
    out.set(1, 320.0F, 40.0F, 100.0F, 60.0F, 19, 0.8F);  // tutta nel padding: scartata
    const auto dets = bp::decode_yolo(out.data.data(), FakeOutput::kChannels, out.anchors, {}, lb, 1280, 720);
    CHECK(dets.size() == 1);
    if (dets.size() == 1) {
        CHECK(near(dets[0].y, 0.0F));
        CHECK(near(dets[0].h, 20.0F / 720.0F));  // (150 - 140) / 0.5 = 20 px visibili
    }
}

void test_decode_invalid_class() {
    FakeOutput out(1);
    out.set(0, 320.0F, 320.0F, 100.0F, 100.0F, 19, 0.9F);
    bp::YoloDecodeParams params;
    params.model_class_id = 80;  // COCO ha classi 0..79
    const bp::Letterbox lb = bp::compute_letterbox(640, 640, 640);
    CHECK(bp::decode_yolo(out.data.data(), FakeOutput::kChannels, 1, params, lb, 640, 640).empty());
}

void test_nms() {
    const std::vector<bp::Detection> boxes = {
        {0.10F, 0.10F, 0.2F, 0.2F, 0, 0.7F},
        {0.11F, 0.10F, 0.2F, 0.2F, 0, 0.9F},  // doppione piu' sicuro della prima
        {0.60F, 0.60F, 0.2F, 0.2F, 0, 0.5F},  // un'altra mucca
    };
    const auto kept = bp::nms(boxes, 0.45F);
    CHECK(kept.size() == 2);
    if (kept.size() == 2) {
        CHECK(near(kept[0].confidence, 0.9F));  // vince la piu' sicura, ordinate per confidenza
        CHECK(near(kept[1].x, 0.60F));
    }
}

} // namespace

void run_yolo_postprocess_tests() {
    test_letterbox();
    test_expected_anchors();
    test_decode_maps_back_to_frame();
    test_decode_clips_padding();
    test_decode_invalid_class();
    test_nms();
}
