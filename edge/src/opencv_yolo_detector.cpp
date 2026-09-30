#include "bp/opencv_yolo_detector.hpp"

#include <opencv2/core.hpp>
#include <opencv2/dnn.hpp>
#include <opencv2/imgproc.hpp>

#include <filesystem>
#include <stdexcept>

namespace bp {

namespace {

// Grigio usato da ultralytics per il padding in addestramento: usare lo stesso valore
// evita che il modello veda bordi "strani".
const cv::Scalar kPadColor(114, 114, 114);

} // namespace

struct OpenCvYoloDetector::Impl {
    cv::dnn::Net net;
    cv::Mat input;  // immagine letterbox input_size x input_size, riusata tra i frame
    cv::Mat blob;
};

OpenCvYoloDetector::OpenCvYoloDetector(const std::string& model_path, int input_size, YoloDecodeParams params)
    : impl_(std::make_unique<Impl>()), input_size_(input_size), params_(params) {
    if (!std::filesystem::is_regular_file(model_path)) {
        throw std::runtime_error(model_path + ": modello non trovato (generalo con tools/export_model.sh)");
    }
    if (input_size <= 0 || input_size % 32 != 0) {
        throw std::runtime_error("--model-size deve essere un multiplo di 32 (es. 320, 640)");
    }
    try {
        impl_->net = cv::dnn::readNetFromONNX(model_path);
    } catch (const cv::Exception& e) {
        throw std::runtime_error(model_path + ": impossibile caricare il modello: " + e.what());
    }
    if (impl_->net.empty()) {
        throw std::runtime_error(model_path + ": modello vuoto");
    }
    impl_->net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
    impl_->net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
    impl_->input.create(input_size, input_size, CV_8UC3);
}

OpenCvYoloDetector::~OpenCvYoloDetector() = default;

std::vector<Detection> OpenCvYoloDetector::detect(const Frame& frame) {
    // Contratto, regola 4: mai fidarsi della dimensione dei dati.
    if (frame.width <= 0 || frame.height <= 0 ||
        frame.data.size() != static_cast<std::size_t>(frame.width) * frame.height * 3) {
        return {};
    }
    // Vista sui pixel del Frame senza copiarli (cv::Mat non possiede la memoria).
    const cv::Mat bgr(frame.height, frame.width, CV_8UC3, const_cast<std::uint8_t*>(frame.data.data()));

    // 1. Letterbox: ridimensiona mantenendo le proporzioni e centra su sfondo grigio.
    const Letterbox lb = compute_letterbox(frame.width, frame.height, input_size_);
    impl_->input.setTo(kPadColor);
    // roi e' una vista sulla zona centrale di input: resize ci scrive dentro, senza riallocare.
    cv::Mat roi = impl_->input(cv::Rect(lb.pad_x, lb.pad_y, lb.new_w, lb.new_h));
    cv::resize(bgr, roi, roi.size(), 0, 0, cv::INTER_LINEAR);

    // 2. Tensore d'ingresso: pixel 0..1 (1/255), BGR->RGB (swapRB), layout NCHW float.
    cv::dnn::blobFromImage(impl_->input, impl_->blob, 1.0 / 255.0, cv::Size(), cv::Scalar(), /*swapRB=*/true,
                           /*crop=*/false, CV_32F);
    impl_->net.setInput(impl_->blob);

    // 3. Inferenza. Output atteso: [1, 4 + classi, anchor].
    cv::Mat out;
    try {
        out = impl_->net.forward();
    } catch (const cv::Exception& e) {
        // Causa tipica: modello esportato a una dimensione diversa da --model-size.
        throw std::runtime_error("inferenza fallita (--model-size corrisponde al modello?): " + e.msg);
    }
    if (out.dims != 3 || out.size[0] != 1 || out.size[1] < 5) {
        throw std::runtime_error("output del modello inatteso: serve un YOLOv8/YOLO11 esportato da ultralytics");
    }
    const int channels = out.size[1];
    const int anchors = out.size[2];
    const int classes = channels - 4;
    if (params_.model_class_id >= classes) {
        // Senza questo controllo decode_yolo restituirebbe zero box: conteggio fermo a 0 senza errori.
        throw std::runtime_error("il modello ha " + std::to_string(classes) + " classi (0.." +
                                 std::to_string(classes - 1) + "), --class-id " +
                                 std::to_string(params_.model_class_id) +
                                 " non esiste (con un modello a classe unica usa --class-id 0)");
    }
    if (anchors != yolo_expected_anchors(input_size_)) {
        throw std::runtime_error("il modello non e' stato esportato a " + std::to_string(input_size_) +
                                 " px: controlla --model-size");
    }
    if (!out.isContinuous()) {
        out = out.clone();
    }

    // 4. Decodifica, riconversione al frame originale, NMS.
    return decode_yolo(out.ptr<float>(), channels, anchors, params_, lb, frame.width, frame.height);
}

} // namespace bp
