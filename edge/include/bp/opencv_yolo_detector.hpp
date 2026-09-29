#pragma once

#include <memory>
#include <string>

#include "bp/detector.hpp"
#include "bp/yolo_postprocess.hpp"

namespace bp {

// Detector VERO: modello YOLOv8/YOLO11 in formato ONNX eseguito con OpenCV DNN (CPU).
// Pipeline: letterbox -> BGR->RGB, 0..1, NCHW -> inferenza -> decode_yolo (vedi yolo_postprocess.hpp).
// Il modello si genera con tools/export_model.sh.
class OpenCvYoloDetector final : public IDetector {
public:
    // input_size: lato del quadrato d'ingresso con cui il modello e' stato esportato (es. 640).
    // Lancia std::runtime_error se il modello non si carica.
    OpenCvYoloDetector(const std::string& model_path, int input_size, YoloDecodeParams params = YoloDecodeParams{});
    ~OpenCvYoloDetector() override;  // definito nel .cpp, dove Impl e' completo

    OpenCvYoloDetector(const OpenCvYoloDetector&) = delete;
    OpenCvYoloDetector& operator=(const OpenCvYoloDetector&) = delete;

    // Lancia std::runtime_error se l'output del modello non ha la forma attesa
    // (es. --model-size sbagliato, o modello non YOLOv8/11).
    std::vector<Detection> detect(const Frame& frame) override;

private:
    // Pimpl: gli header OpenCV restano nel .cpp.
    struct Impl;
    std::unique_ptr<Impl> impl_;
    int input_size_;
    YoloDecodeParams params_;
};

} // namespace bp
