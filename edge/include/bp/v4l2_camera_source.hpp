#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "bp/frame_source.hpp"

namespace bp {

// Sorgente VERA: camera USB/CSI su Linux tramite V4L2, senza librerie esterne.
// Chiede alla camera il formato YUYV (non compresso) e lo converte in BGR.
// La camera puo' scegliere una risoluzione diversa da quella chiesta:
// dopo la costruzione width()/height() dicono quella effettiva.
class V4l2CameraSource final : public FrameSource {
public:
    // Lancia std::runtime_error se la camera non si apre o non supporta YUYV.
    V4l2CameraSource(const std::string& device, int width, int height, double fps);
    ~V4l2CameraSource() override;

    // Possiede un file descriptor e memoria mappata: niente copie.
    V4l2CameraSource(const V4l2CameraSource&) = delete;
    V4l2CameraSource& operator=(const V4l2CameraSource&) = delete;

    bool read(Frame& out) override;

    int width() const { return width_; }
    int height() const { return height_; }
    double fps() const { return fps_; }

private:
    // Buffer condiviso col driver (mmap): la camera ci scrive, noi leggiamo.
    struct MappedBuffer {
        void* start = nullptr;
        std::size_t length = 0;
    };

    void close_device();

    std::string device_;
    int fd_ = -1;
    int width_ = 0;
    int height_ = 0;
    int bytes_per_line_ = 0;  // puo' essere > width*2 se il driver aggiunge padding
    double fps_ = 0.0;
    bool streaming_ = false;
    double start_s_ = 0.0;    // CLOCK_MONOTONIC all'avvio, per timestamp relativi
    std::vector<MappedBuffer> buffers_;
};

} // namespace bp
