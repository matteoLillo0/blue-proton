#include "bp/v4l2_camera_source.hpp"

#include <fcntl.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <ctime>
#include <iostream>
#include <stdexcept>

namespace bp {

namespace {

constexpr unsigned kBufferCount = 4;   // abbastanza per non perdere frame, poca memoria
constexpr int kReadTimeoutMs = 2000;   // oltre questo consideriamo la camera bloccata

// ioctl riprovato se interrotto da un segnale (es. Ctrl+C arriva durante la chiamata).
int xioctl(int fd, unsigned long request, void* arg) {
    int r = 0;
    do {
        r = ioctl(fd, request, arg);
    } while (r == -1 && errno == EINTR);
    return r;
}

[[noreturn]] void fail(const std::string& device, const std::string& what) {
    throw std::runtime_error(device + ": " + what + " (" + std::strerror(errno) + ")");
}

double monotonic_now_s() {
    timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<double>(ts.tv_sec) + static_cast<double>(ts.tv_nsec) * 1e-9;
}

std::uint8_t clamp_u8(int v) { return static_cast<std::uint8_t>(std::clamp(v, 0, 255)); }

// YUYV 4:2:2 -> BGR, formula BT.601 "limited range" (quella delle webcam) in aritmetica intera.
// Ogni gruppo di 4 byte [Y0 U Y1 V] descrive 2 pixel che condividono lo stesso colore (U, V).
void yuyv_to_bgr(const std::uint8_t* src, int width, int height, int src_stride, std::uint8_t* dst) {
    for (int row = 0; row < height; ++row) {
        const std::uint8_t* s = src + static_cast<std::size_t>(row) * src_stride;
        std::uint8_t* d = dst + static_cast<std::size_t>(row) * width * 3;
        for (int col = 0; col + 1 < width; col += 2, s += 4, d += 6) {
            const int u = s[1] - 128;
            const int v = s[3] - 128;
            const int ruv = 409 * v;
            const int guv = -100 * u - 208 * v;
            const int buv = 516 * u;
            for (int k = 0; k < 2; ++k) {
                const int c = 298 * (s[k * 2] - 16) + 128;  // +128: arrotonda invece di troncare
                d[k * 3 + 0] = clamp_u8((c + buv) >> 8);
                d[k * 3 + 1] = clamp_u8((c + guv) >> 8);
                d[k * 3 + 2] = clamp_u8((c + ruv) >> 8);
            }
        }
    }
}

} // namespace

V4l2CameraSource::V4l2CameraSource(const std::string& device, int width, int height, double fps)
    : device_(device) {
    // Il distruttore non viene chiamato se il costruttore lancia: puliamo a mano.
    try {
        fd_ = open(device.c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
        if (fd_ < 0) {
            fail(device, "impossibile aprire la camera");
        }

        v4l2_capability cap{};
        if (xioctl(fd_, VIDIOC_QUERYCAP, &cap) < 0) {
            fail(device, "non e' un dispositivo V4L2");
        }
        const auto caps = (cap.capabilities & V4L2_CAP_DEVICE_CAPS) != 0 ? cap.device_caps : cap.capabilities;
        if ((caps & V4L2_CAP_VIDEO_CAPTURE) == 0 || (caps & V4L2_CAP_STREAMING) == 0) {
            errno = ENOTSUP;
            fail(device, "il dispositivo non supporta cattura video in streaming");
        }

        // Formato: chiediamo YUYV alla risoluzione voluta; il driver sceglie la piu' vicina.
        v4l2_format fmt{};
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        fmt.fmt.pix.width = static_cast<__u32>(width);
        fmt.fmt.pix.height = static_cast<__u32>(height);
        fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
        fmt.fmt.pix.field = V4L2_FIELD_NONE;
        if (xioctl(fd_, VIDIOC_S_FMT, &fmt) < 0) {
            fail(device, "impossibile impostare il formato");
        }
        if (fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_YUYV) {
            errno = ENOTSUP;
            fail(device, "la camera non supporta YUYV");
        }
        width_ = static_cast<int>(fmt.fmt.pix.width);
        height_ = static_cast<int>(fmt.fmt.pix.height);
        bytes_per_line_ = std::max(static_cast<int>(fmt.fmt.pix.bytesperline), width_ * 2);

        // FPS: come per la risoluzione, e' una richiesta. Non tutte le camere lo supportano.
        v4l2_streamparm parm{};
        parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        parm.parm.capture.timeperframe.numerator = 1000;
        parm.parm.capture.timeperframe.denominator = static_cast<__u32>(std::lround(fps * 1000.0));
        if (xioctl(fd_, VIDIOC_S_PARM, &parm) == 0 && parm.parm.capture.timeperframe.numerator != 0) {
            fps_ = static_cast<double>(parm.parm.capture.timeperframe.denominator) /
                   parm.parm.capture.timeperframe.numerator;
        } else {
            fps_ = fps;
        }

        // Buffer in memoria condivisa col driver: niente copia kernel -> utente.
        v4l2_requestbuffers req{};
        req.count = kBufferCount;
        req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_MMAP;
        if (xioctl(fd_, VIDIOC_REQBUFS, &req) < 0 || req.count < 2) {
            fail(device, "impossibile allocare i buffer");
        }
        buffers_.resize(req.count);
        for (unsigned i = 0; i < req.count; ++i) {
            v4l2_buffer buf{};
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;
            if (xioctl(fd_, VIDIOC_QUERYBUF, &buf) < 0) {
                fail(device, "VIDIOC_QUERYBUF fallito");
            }
            void* p = mmap(nullptr, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, buf.m.offset);
            if (p == MAP_FAILED) {
                fail(device, "mmap fallito");
            }
            buffers_[i] = {p, buf.length};
            if (xioctl(fd_, VIDIOC_QBUF, &buf) < 0) {
                fail(device, "VIDIOC_QBUF fallito");
            }
        }

        v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (xioctl(fd_, VIDIOC_STREAMON, &type) < 0) {
            fail(device, "impossibile avviare lo streaming");
        }
        streaming_ = true;
        start_s_ = monotonic_now_s();
    } catch (...) {
        close_device();
        throw;
    }
}

V4l2CameraSource::~V4l2CameraSource() { close_device(); }

void V4l2CameraSource::close_device() {
    if (streaming_) {
        v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        xioctl(fd_, VIDIOC_STREAMOFF, &type);
        streaming_ = false;
    }
    for (auto& b : buffers_) {
        if (b.start != nullptr) {
            munmap(b.start, b.length);
        }
    }
    buffers_.clear();
    if (fd_ >= 0) {
        close(fd_);
        fd_ = -1;
    }
}

bool V4l2CameraSource::read(Frame& out) {
    // Frame corrotti (flag di errore o incompleti) vengono saltati: si riprova col prossimo.
    for (;;) {
        // Aspettiamo che il driver abbia un frame pronto, con timeout per non bloccarci
        // per sempre se la camera viene staccata.
        pollfd pfd{fd_, POLLIN, 0};
        int r = 0;
        do {
            r = poll(&pfd, 1, kReadTimeoutMs);
        } while (r == -1 && errno == EINTR);
        if (r == 0) {
            std::cerr << device_ << ": nessun frame da " << kReadTimeoutMs << " ms\n";
            return false;
        }
        if (r < 0 || (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
            std::cerr << device_ << ": errore in attesa del frame (camera scollegata?)\n";
            return false;
        }

        v4l2_buffer buf{};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        if (xioctl(fd_, VIDIOC_DQBUF, &buf) < 0) {
            std::cerr << device_ << ": VIDIOC_DQBUF fallito (" << std::strerror(errno) << ")\n";
            return false;
        }
        // Se il detector e' piu' lento della camera, il driver accumula frame vecchi:
        // li scartiamo e teniamo solo il piu' recente, cosi' contiamo cio' che succede ADESSO.
        // Il device e' non bloccante: quando non ci sono altri frame pronti DQBUF da' EAGAIN.
        for (;;) {
            v4l2_buffer newer{};
            newer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            newer.memory = V4L2_MEMORY_MMAP;
            if (xioctl(fd_, VIDIOC_DQBUF, &newer) < 0) {
                if (errno == EAGAIN) {
                    break;
                }
                std::cerr << device_ << ": VIDIOC_DQBUF fallito (" << std::strerror(errno) << ")\n";
                return false;
            }
            if (xioctl(fd_, VIDIOC_QBUF, &buf) < 0) {  // il vecchio torna al driver
                std::cerr << device_ << ": VIDIOC_QBUF fallito (" << std::strerror(errno) << ")\n";
                return false;
            }
            buf = newer;
        }

        const std::size_t needed = static_cast<std::size_t>(bytes_per_line_) * height_;
        const bool complete = buf.bytesused >= needed && (buf.flags & V4L2_BUF_FLAG_ERROR) == 0;
        if (complete) {
            out.width = width_;
            out.height = height_;
            out.data.resize(static_cast<std::size_t>(width_) * height_ * 3);  // riusa la capacita' gia' allocata
            yuyv_to_bgr(static_cast<const std::uint8_t*>(buffers_[buf.index].start), width_, height_, bytes_per_line_,
                        out.data.data());
            // Il driver marca ogni frame con l'istante di cattura, piu' preciso di "adesso".
            const bool monotonic = (buf.flags & V4L2_BUF_FLAG_TIMESTAMP_MASK) == V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
            const double t = monotonic ? static_cast<double>(buf.timestamp.tv_sec) +
                                             static_cast<double>(buf.timestamp.tv_usec) * 1e-6
                                       : monotonic_now_s();
            out.timestamp_s = std::max(0.0, t - start_s_);
        }

        // Restituiamo SEMPRE il buffer al driver, altrimenti dopo kBufferCount frame si blocca.
        if (xioctl(fd_, VIDIOC_QBUF, &buf) < 0) {
            std::cerr << device_ << ": VIDIOC_QBUF fallito (" << std::strerror(errno) << ")\n";
            return false;
        }
        if (complete) {
            return true;
        }
    }
}

} // namespace bp
